
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>

using namespace std;

#ifdef __ICC
#define INLINE __forceinline
#else
#define INLINE inline
#endif


#if defined(__i386__) || defined(__x86_64__)

#define LOCK "lock ; "


static INLINE char CAS1(volatile void* addr, volatile int value, int newvalue)
{
    register char ret;
    __asm__ __volatile__ (
						  "# CAS \n\t"
						  LOCK "cmpxchg %2, (%1) \n\t"
						  "sete %0               \n\t"
						  : "=a" (ret)
						  : "c" (addr), "d" (newvalue), "a" (value)
                          : "memory"
						  );
    return ret;
}

static INLINE int atomic_xadd(volatile int* atomic, int val) 
{ 
    register int result;
    __asm__ __volatile__ ("# atomic_xadd \n\t"
                          LOCK "xaddl %0,%1 \n\t"
                          : "=r" (result), "=m" (*atomic) 
                          : "0" (val), "m" (*atomic));
    return result;
} 

#endif


/*
static INLINE int INC_ATOMIC(volatile int* val)
{
    int actual;
    do {
        actual = *val;
    } while (!CAS1(val, actual, actual + 1));
    return actual;
}

static INLINE int DEC_ATOMIC(volatile int* val)
{
    int actual;
    do {
        actual = *val;
    } while (!CAS1(val, actual, actual - 1));
    return actual;
}
*/

static INLINE int INC_ATOMIC(volatile int* val)
{
    return atomic_xadd(val, 1);
}
 
static INLINE int DEC_ATOMIC(volatile int* val)
{
    return atomic_xadd(val, -1);
}
 
// To be used in lock-free queue
struct AtomicCounter
{
    union {
        struct {
            short fHead;	
            short fTail;	
        }
        scounter;
        int fValue;
    }info;
    
	INLINE AtomicCounter()
	{
        info.fValue = 0;
    }
     
 	INLINE  AtomicCounter& operator=(AtomicCounter& obj)
    {
        info.fValue = obj.info.fValue;
        return *this;
    }
    
	INLINE  AtomicCounter& operator=(volatile AtomicCounter& obj)
	{
        info.fValue = obj.info.fValue;
        return *this;
    }
    
};

int get_max_cpu()
{
    return sysconf(_SC_NPROCESSORS_ONLN);
}

static int GetPID()
{
#ifdef WIN32
    return  _getpid();
#else
    return getpid();
#endif
}

#define Value(e) (e).info.fValue

#define Head(e) (e).info.scounter.fHead
#define IncHead(e) (e).info.scounter.fHead++
#define DecHead(e) (e).info.scounter.fHead--

#define Tail(e) (e).info.scounter.fTail
#define IncTail(e) (e).info.scounter.fTail++
#define DecTail(e) (e).info.scounter.fTail--

#define THREAD_SIZE 256
#define QUEUE_SIZE 256

#define WORK_STEALING_INDEX 0
#define LAST_TASK_INDEX 1

class TaskQueue 
{
    private:
    
        int fTaskList[QUEUE_SIZE];
        volatile AtomicCounter fCounter;
    
    public:
  
        INLINE TaskQueue()
        {
            for (int i = 0; i < QUEUE_SIZE; i++) {
                fTaskList[i] = -1;
            }
            
            int val;
            do {
                val = gNumQueue;
                gTaskQueueList[val] = this;
            } while (!CAS1(&gNumQueue, val, val + 1));
            
        }
         
        INLINE void PushHead(int item)
        {
            fTaskList[Head(fCounter)] = item;
            IncHead(fCounter);
        }
        
        INLINE int PopHead()
        {
            AtomicCounter old_val;
            AtomicCounter new_val;
            
            do {
                old_val = fCounter;
                new_val = fCounter;
                if (Head(old_val) == Tail(old_val)) {
                    return WORK_STEALING_INDEX;
                } else {
                    DecHead(new_val);
                }
            } while (!CAS1(&fCounter, Value(old_val), Value(new_val)));
            
            return fTaskList[Head(old_val) - 1];
        }
        
        INLINE int PopTail()
        {
            AtomicCounter old_val;
            AtomicCounter new_val;
            
            do {
                old_val = fCounter;
                new_val = fCounter;
                if (Head(old_val) == Tail(old_val)) {
                   return WORK_STEALING_INDEX;
                } else {
                    IncTail(new_val);
                }
            } while (!CAS1(&fCounter, Value(old_val), Value(new_val)));
            
            return fTaskList[Tail(old_val)];
        }
        
        static INLINE int GetNextTask(int thread)
        {
            int tasknum;
            int numqueue = gNumQueue;   // Important : use local variable
            for (int i = 0; i < numqueue; i++) {
                if ((i != thread) && (tasknum = gTaskQueueList[i]->PopTail()) != WORK_STEALING_INDEX) {
                    return tasknum;    // Task is found
                }
            }
            
            /*
            int res, i = 0;
            while (i++ < 10000) {
                res += 10;
            }
            */
            
            return WORK_STEALING_INDEX;    // Otherwise will try "workstealing" again next cycle...
        }
        
        INLINE void InitTaskList(int task_list_size, int* task_list, int thread_num, int cur_thread, int& tasknum)
        {
            int task_slice = task_list_size / thread_num;
            int task_slice_rest = task_list_size % thread_num;

            if (task_slice == 0) {
                // Each thread directly executes one task
                tasknum = task_list[cur_thread];
                // Thread 0 takes remaining ready tasks 
                if (cur_thread == 0) { 
                    for (int index = 0; index < task_slice_rest - thread_num; index++) {
                        PushHead(task_list[task_slice_rest + index]);
                    }
                }
            } else {
                // Each thread takes a part of ready tasks
                int index;
                for (index = 0; index < task_slice - 1; index++) {
                    PushHead(task_list[cur_thread * task_slice + index]);
                }
                // Each thread directly executes one task 
                tasknum = task_list[cur_thread * task_slice + index];
                // Thread 0 takes remaining ready tasks 
                if (cur_thread == 0) {
                    for (index = 0; index < task_slice_rest; index++) {
                        PushHead(task_list[thread_num * task_slice + index]);
                    }
                }
            }
        }
        
        static INLINE void Init()
        {
            gNumQueue = 0;
            for (int i = 0; i < THREAD_SIZE; i++) {
                gTaskQueueList[i] = 0;
            }
        }
         
        static volatile int gNumQueue;
        static TaskQueue* gTaskQueueList[THREAD_SIZE];
     
};

struct TaskGraph 
{
    volatile int gTaskList[QUEUE_SIZE];
    
    TaskGraph()
    {
        for (int i = 0; i < QUEUE_SIZE; i++) {
            gTaskList[i] = 0;
        } 
    }

    INLINE void InitTask(int task, int val)
    {
        gTaskList[task] = val;
    }
      
    INLINE void ActivateOutputTask(TaskQueue& queue, int task, int& tasknum)
    {
        if (DEC_ATOMIC(&gTaskList[task]) == 1) {
            if (tasknum == WORK_STEALING_INDEX) {
                tasknum = task;
            } else {
                queue.PushHead(task);
            }
        }    
    }
      
    INLINE void GetReadyTask(TaskQueue& queue, int& tasknum)
    {
        if (tasknum == WORK_STEALING_INDEX) {
            tasknum = queue.PopHead();
        }
    }
    
    INLINE void ActivateOutputTask(TaskQueue& queue, int task)
    {
        if (DEC_ATOMIC(&gTaskList[task]) == 1) {
            queue.PushHead(task);
        }
    }
    
    INLINE void ActivateOneOutputTask(TaskQueue& queue, int task, int& tasknum)
    {
        if (DEC_ATOMIC(&gTaskList[task]) == 1) {
            tasknum = task;
        } else {
            tasknum = queue.PopHead(); 
        }
    }
};

#define THREAD_POOL_SIZE 16
#define JACK_SCHED_POLICY SCHED_FIFO

/* use 512KB stack per thread - the default is way too high to be feasible
 * with mlockall() on many systems */
#define THREAD_STACK 524288


#ifdef __APPLE__

#import <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacTypes.h>
#include <mach/thread_policy.h>
#include <mach/thread_act.h>

#define THREAD_SET_PRIORITY         0
#define THREAD_SCHEDULED_PRIORITY   1

static UInt32 GetThreadPriority(pthread_t thread, int inWhichPriority);

// returns the thread's priority as it was last set by the API
static UInt32 GetThreadSetPriority(pthread_t thread)
{
    return GetThreadPriority(thread, THREAD_SET_PRIORITY);
}

// returns the thread's priority as it was last scheduled by the Kernel
static UInt32 GetThreadScheduledPriority(pthread_t thread)
{
    return GetThreadPriority(thread, THREAD_SCHEDULED_PRIORITY);
}

static int SetThreadToPriority(pthread_t thread, UInt32 inPriority, Boolean inIsFixed, UInt64 period, UInt64 computation, UInt64 constraint)
{
    if (inPriority == 96) {
        // REAL-TIME / TIME-CONSTRAINT THREAD
        thread_time_constraint_policy_data_t theTCPolicy;
        theTCPolicy.period = period;
        theTCPolicy.computation = computation;
        theTCPolicy.constraint = constraint;
        theTCPolicy.preemptible = true;
        kern_return_t res = thread_policy_set(pthread_mach_thread_np(thread), THREAD_TIME_CONSTRAINT_POLICY, (thread_policy_t)&theTCPolicy, THREAD_TIME_CONSTRAINT_POLICY_COUNT);
        return (res == KERN_SUCCESS) ? 0 : -1;
    } else {
        // OTHER THREADS
        thread_extended_policy_data_t theFixedPolicy;
        thread_precedence_policy_data_t thePrecedencePolicy;
        SInt32 relativePriority;
        
        // [1] SET FIXED / NOT FIXED
        theFixedPolicy.timeshare = !inIsFixed;
        thread_policy_set(pthread_mach_thread_np(thread), THREAD_EXTENDED_POLICY, (thread_policy_t)&theFixedPolicy, THREAD_EXTENDED_POLICY_COUNT);
        
        // [2] SET PRECEDENCE
        // N.B.: We expect that if thread A created thread B, and the program wishes to change
        // the priority of thread B, then the call to change the priority of thread B must be
        // made by thread A.
        // This assumption allows us to use pthread_self() to correctly calculate the priority
        // of the feeder thread (since precedency policy's importance is relative to the
        // spawning thread's priority.)
        relativePriority = inPriority - GetThreadSetPriority(pthread_self());
        
        thePrecedencePolicy.importance = relativePriority;
        kern_return_t res = thread_policy_set(pthread_mach_thread_np(thread), THREAD_PRECEDENCE_POLICY, (thread_policy_t)&thePrecedencePolicy, THREAD_PRECEDENCE_POLICY_COUNT);
        return (res == KERN_SUCCESS) ? 0 : -1;
    }
}

static UInt32 GetThreadPriority(pthread_t thread, int inWhichPriority)
{
    thread_basic_info_data_t threadInfo;
    policy_info_data_t thePolicyInfo;
    unsigned int count;
    
    // get basic info
    count = THREAD_BASIC_INFO_COUNT;
    thread_info(pthread_mach_thread_np(thread), THREAD_BASIC_INFO, (thread_info_t)&threadInfo, &count);
    
    switch (threadInfo.policy) {
        case POLICY_TIMESHARE:
            count = POLICY_TIMESHARE_INFO_COUNT;
            thread_info(pthread_mach_thread_np(thread), THREAD_SCHED_TIMESHARE_INFO, (thread_info_t)&(thePolicyInfo.ts), &count);
            if (inWhichPriority == THREAD_SCHEDULED_PRIORITY) {
                return thePolicyInfo.ts.cur_priority;
            } else {
                return thePolicyInfo.ts.base_priority;
            }
            break;
            
        case POLICY_FIFO:
            count = POLICY_FIFO_INFO_COUNT;
            thread_info(pthread_mach_thread_np(thread), THREAD_SCHED_FIFO_INFO, (thread_info_t)&(thePolicyInfo.fifo), &count);
            if ((thePolicyInfo.fifo.depressed) && (inWhichPriority == THREAD_SCHEDULED_PRIORITY)) {
                return thePolicyInfo.fifo.depress_priority;
            }
            return thePolicyInfo.fifo.base_priority;
            break;
            
        case POLICY_RR:
            count = POLICY_RR_INFO_COUNT;
            thread_info(pthread_mach_thread_np(thread), THREAD_SCHED_RR_INFO, (thread_info_t)&(thePolicyInfo.rr), &count);
            if ((thePolicyInfo.rr.depressed) && (inWhichPriority == THREAD_SCHEDULED_PRIORITY)) {
                return thePolicyInfo.rr.depress_priority;
            }
            return thePolicyInfo.rr.base_priority;
            break;
    }
    
    return 0;
}

static int GetParams(pthread_t thread, UInt64* period, UInt64* computation, UInt64* constraint)
{
    thread_time_constraint_policy_data_t theTCPolicy;
    mach_msg_type_number_t count = THREAD_TIME_CONSTRAINT_POLICY_COUNT;
    boolean_t get_default = false;
    
    kern_return_t res = thread_policy_get(pthread_mach_thread_np(thread),
                                          THREAD_TIME_CONSTRAINT_POLICY,
                                          (thread_policy_t)&theTCPolicy,
                                          &count,
                                          &get_default);
    if (res == KERN_SUCCESS) {
        *period = theTCPolicy.period;
        *computation = theTCPolicy.computation;
        *constraint = theTCPolicy.constraint;
        return 0;
    } else {
        return -1;
    }
}

static UInt64 period = 0;
static UInt64 computation = 0;
static UInt64 constraint = 0;

INLINE void GetRealTime()
{
    if (period == 0) {
        GetParams(pthread_self(), &period, &computation, &constraint);
    }
}

INLINE void SetRealTime()
{
    SetThreadToPriority(pthread_self(), 96, true, period, computation, constraint);
}

#endif

#ifdef __linux__

static int faust_sched_policy = -1;
static struct sched_param faust_rt_param; 

INLINE void GetRealTime()
{
    if (faust_sched_policy == -1) {
        pthread_attr_t attributes;
        pthread_attr_init(&attributes);
        memset(&faust_rt_param, 0, sizeof(faust_rt_param));
        pthread_attr_getschedpolicy(&attributes, &faust_sched_policy);
        pthread_attr_getschedparam(&attributes, &faust_rt_param);
    }
}

INLINE void SetRealTime()
{
    pthread_setschedparam(pthread_self(), faust_sched_policy, &faust_rt_param);
}

#endif

static INLINE unsigned long long int DSP_rdtsc(void)
{
    unsigned long long int x;
    __asm__ volatile (".byte 0x0f, 0x31" : "=A" (x));
    return x;
}

#define KDSPMESURE 50

static INLINE int Range(int min, int max, int val)
{
    if (val < min) {
        return min;
    } else if (val > max) {
        return max;
    } else {
        return val;
    }
}

struct Runnable {
    
    unsigned long long int fTiming[KDSPMESURE];
    unsigned long long int fStart;
    unsigned long long int fStop;
    int fCounter;
    float fOldMean;
    int fOldfDynamicNumThreads;
    bool fDynAdapt;
    
    virtual void computeThread(int cur_thread) = 0;
    
    Runnable():fCounter(0), fOldMean(1000000000.f), fOldfDynamicNumThreads(1)
    {
        for (int i = 0; i < KDSPMESURE; i++) {
            fTiming[i] = 0;
        }
        
        fDynAdapt = getenv("OMP_DYN_THREAD") ? atoi(getenv("OMP_DYN_THREAD")) : false;
    }
    
    INLINE float ComputeMean()
    {
        float mean = 0;
        for (int i = 0; i < KDSPMESURE; i++) {
            mean += float(fTiming[i]);
        }
        mean /= float(KDSPMESURE);
        return mean;
    }
    
    INLINE void StartMeasure()
    {
        if (!fDynAdapt)
            return;
        
        fStart = DSP_rdtsc();
    }
     
    INLINE void StopMeasure(int staticthreadnum, int& dynthreadnum)
    {
        if (!fDynAdapt)
            return;
        
        fStop = DSP_rdtsc();
        fCounter = (fCounter + 1) % KDSPMESURE;
        if (fCounter == 0) {
            float mean = ComputeMean();
            if (fabs(mean - fOldMean) > 5000) {
                if (mean > fOldMean) { // Worse...
                    //printf("Worse %f %f\n", mean, fOldMean);
                    if (fOldfDynamicNumThreads > dynthreadnum) {
                        fOldfDynamicNumThreads = dynthreadnum;
                        dynthreadnum += 1;
                    } else {
                        fOldfDynamicNumThreads = dynthreadnum;
                        dynthreadnum -= 1;
                    }
                 } else { // Better...
                    //printf("Better %f %f\n", mean, fOldMean);
                    if (fOldfDynamicNumThreads > dynthreadnum) {
                        fOldfDynamicNumThreads = dynthreadnum;
                        dynthreadnum -= 1;
                    } else {
                        fOldfDynamicNumThreads = dynthreadnum;
                        dynthreadnum += 1;
                    }
                }
                fOldMean = mean;
                dynthreadnum = Range(1, staticthreadnum, dynthreadnum);
                //printf("dynthreadnum %d\n", dynthreadnum);
            }
        }
        fTiming[fCounter] = fStop - fStart; 
    }
};

struct DSPThread;

struct DSPThreadPool {
    
    DSPThread* fThreadPool[THREAD_POOL_SIZE];
    int fThreadCount; 
    volatile int fCurThreadCount;
      
    DSPThreadPool();
    ~DSPThreadPool();
    
    void StartAll(int num, bool realtime, Runnable* runnable);
    void StopAll();
    void SignalAll(int num);
    
    void SignalOne();
    bool IsFinished();
    
};

struct DSPThread {

    pthread_t fThread;
    DSPThreadPool* fThreadPool;
    Runnable* fRunnable;
    sem_t* fSemaphore;
    char fName[128];
    bool fRealTime;
    int fNum;
    
    DSPThread(int num, Runnable* runnable, DSPThreadPool* pool)
    {
        fNum = num;
        fThreadPool = pool;
        fRunnable = runnable;
        fRealTime = false;
        
        sprintf(fName, "faust_sem_%d_%ld", GetPID(), int(this));
        
        if ((fSemaphore = sem_open(fName, O_CREAT, 0777, 0)) == (sem_t*)SEM_FAILED) {
            printf("Allocate: can't check in named semaphore name = %s err = %s", fName, strerror(errno));
        }
    }

    virtual ~DSPThread()
    {
        sem_unlink(fName);
        sem_close(fSemaphore);
    }
    
    void Run()
    {
        sem_wait(fSemaphore);
        fRunnable->computeThread(fNum + 1);
        fThreadPool->SignalOne();
    }
    
    static void* ThreadHandler(void* arg)
    {
        DSPThread* thread = static_cast<DSPThread*>(arg);
         
        // One "dummy" cycle to setup thread
        if (thread->fRealTime) {
            thread->Run();
            SetRealTime();
        }
                  
        while (true) {
            thread->Run();
        }
        
        return NULL;
    }
    
    int Start(bool realtime)
    {
        pthread_attr_t attributes;
        struct sched_param rt_param;
        pthread_attr_init(&attributes);
        
        int priority = 60; // TODO
        int res;
        
        if (realtime) {
            fRealTime = true;
        }else {
            fRealTime = getenv("OMP_REALTIME") ? atoi(getenv("OMP_REALTIME")) : true;
        }
                               
        if ((res = pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_JOINABLE))) {
            printf("Cannot request joinable thread creation for real-time thread res = %d err = %s\n", res, strerror(errno));
            return -1;
        }

        if ((res = pthread_attr_setscope(&attributes, PTHREAD_SCOPE_SYSTEM))) {
            printf("Cannot set scheduling scope for real-time thread res = %d err = %s\n", res, strerror(errno));
            return -1;
        }

        if (realtime) {
            
            if ((res = pthread_attr_setinheritsched(&attributes, PTHREAD_EXPLICIT_SCHED))) {
                printf("Cannot request explicit scheduling for RT thread res = %d err = %s\n", res, strerror(errno));
                return -1;
            }
        
            if ((res = pthread_attr_setschedpolicy(&attributes, JACK_SCHED_POLICY))) {
                printf("Cannot set RR scheduling class for RT thread res = %d err = %s\n", res, strerror(errno));
                return -1;
            }
            
            memset(&rt_param, 0, sizeof(rt_param));
            rt_param.sched_priority = priority;

            if ((res = pthread_attr_setschedparam(&attributes, &rt_param))) {
                printf("Cannot set scheduling priority for RT thread res = %d err = %s\n", res, strerror(errno));
                return -1;
            }

        } else {
            
            if ((res = pthread_attr_setinheritsched(&attributes, PTHREAD_INHERIT_SCHED))) {
                printf("Cannot request explicit scheduling for RT thread res = %d err = %s\n", res, strerror(errno));
                return -1;
            }
        }
     
        if ((res = pthread_attr_setstacksize(&attributes, THREAD_STACK))) {
            printf("Cannot set thread stack size res = %d err = %s\n", res, strerror(errno));
            return -1;
        }
        
        if ((res = pthread_create(&fThread, &attributes, ThreadHandler, this))) {
            printf("Cannot create thread res = %d err = %s\n", res, strerror(errno));
            return -1;
        }

        pthread_attr_destroy(&attributes);
        return 0;
    }
    
    void Signal(bool stop)
    {
        sem_post(fSemaphore);
    }
    
    void Cancel()
    {
        pthread_cancel(fThread);
        pthread_join(fThread, NULL);
    }

};

DSPThreadPool::DSPThreadPool()
{
    for (int i = 0; i < THREAD_POOL_SIZE; i++) {
        fThreadPool[i] = NULL;
    }
    fThreadCount = 0;
    fCurThreadCount = 0;
}

DSPThreadPool::~DSPThreadPool()
{
    StopAll();
    
    for (int i = 0; i < fThreadCount; i++) {
        assert(fThreadPool[i]);
        delete(fThreadPool[i]);
        fThreadPool[i] = NULL;
    }
    
    fThreadCount = 0;
 }

void DSPThreadPool::StartAll(int num, bool realtime, Runnable* runnable)
{
    if (fThreadCount == 0) {  // Protection for multiple call...  (like LADSPA plug-ins in Ardour)
        for (int i = 0; i < num; i++) {
            fThreadPool[i] = new DSPThread(i, runnable, this);
            assert(fThreadPool[i]->Start(realtime) == 0);
            fThreadCount++;
        }
    }
}

void DSPThreadPool::StopAll()
{
    for (int i = 0; i < fThreadCount; i++) {
        assert(fThreadPool[i]);
        fThreadPool[i]->Cancel();
    }
}

void DSPThreadPool::SignalAll(int num)
{
    assert(num <= fThreadCount);
    fCurThreadCount = num;
        
    for (int i = 0; i < num; i++) {  // Important : use local num here...
        assert(fThreadPool[i]);
        fThreadPool[i]->Signal(false);
    }
}

void DSPThreadPool::SignalOne()
{
    DEC_ATOMIC(&fCurThreadCount);
}

bool DSPThreadPool::IsFinished()
{
    return (fCurThreadCount == 0);
}

// Globals
volatile int TaskQueue::gNumQueue = 0;
TaskQueue* TaskQueue::gTaskQueueList[QUEUE_SIZE] = {0};

