#ifndef __TSyntax__
#define __TSyntax__

#include "TType.hh"
#include "TSignal.hh"
#include "TValue.hh"
#include "TIndex.hh"

string getFreshID(const string& prefix);

// Index language
TIndex* MR_INT(int index);
TIndex* MR_VAR(const string& name);

TIndex* MR_ADD(TIndex* v1, TIndex* v2);
TIndex* MR_SUB(TIndex* v1, TIndex* v2);
TIndex* MR_MUL(TIndex* v1, TIndex* v2);
TIndex* MR_DIV(TIndex* v1, TIndex* v2);
TIndex* MR_MOD(TIndex* v1, TIndex* v2);

// Type language
TType*  MR_VECTOR_TYPE(TType* type, int size);
TType*  MR_INT_TYPE();
TType*  MR_FLOAT_TYPE();

// Address language
TVector* MR_VECTOR(const string& name, TType* type);
TAddress* MR_INDEX_ADDRESS(TAddress* address, TIndex* id);
TAddress* MR_CAST_ADDRESS(TAddress* address, TType* type);

// Statement language
TDeclareStatement* MR_DEC(TVector* vector);
TBlockStatement* MR_BLOCK();
TBlockStatement* MR_PUSH_BLOCK(TBlockStatement* block, TStatement* statement);
TStoreStatement* MR_STORE(TAddress* addr, TValue* val);
TLoopStatement* MR_LOOP(int size, TIndex* index, TBlockStatement* code);
TSubLoopStatement* MR_SUBLOOP(int size, TIndex* index, TBlockStatement* code);
TIfStatement* MR_IF(TIndex* test, TBlockStatement* code);

// Value language
TNullValue* MR_NULL();
TFloatValue* MR_FLOAT_VAL(float val);
TIntValue* MR_INT_VAL(int val);

TLoadValue* MR_LOAD(TAddress* addr);

TPrimOpValue* MR_ADD(TValue* v1, TValue* v2);
TPrimOpValue* MR_SUB(TValue* v1, TValue* v2);
TPrimOpValue* MR_MUL(TValue* v1, TValue* v2);
TPrimOpValue* MR_DIV(TValue* v1, TValue* v2);
TPrimOpValue* MR_OP(TValue* v1, TValue* v2, const string& op);

void CHECK_TYPE(TType* type1, TType* type2);

extern bool gPrim;
extern bool gVec;
extern bool gSer;
extern bool gConcat;

#endif

