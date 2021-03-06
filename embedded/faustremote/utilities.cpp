/************************************************************************
 ************************************************************************
 FAUST compiler
 Copyright (C) 2003-2013 GRAME, Centre National de Creation Musicale
 ---------------------------------------------------------------------
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 ************************************************************************
 ************************************************************************/

#include "utilities.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <iostream> 
#include <fstream>

//Returns the content of a file passed in path
string pathToContent(const string& path)
{
    ifstream file(path.c_str(), std::ifstream::binary);
    
    file.seekg (0, file.end);
    int size = file.tellg();
    file.seekg (0, file.beg);
    
    // And allocate buffer to that a single line can be read...
    char* buffer = new char[size + 1];
    file.read(buffer, size);
    
    // Terminate the string
    buffer[size] = 0;
    string result = buffer;
    file.close();
    delete [] buffer;
    return result;
}

//Verify if the word is a number
bool isInt(const char* word){
    
    bool returning = true;
    
    for (size_t i = 0; i < strlen(word); i++){
        if (!isdigit(word[i])){
            returning = false;
            break;
        }
    }
    return returning;
}

long lopt(char *argv[], const char *name, long def)
{
	int	i;
	for (i = 0; argv[i]; i++) if (!strcmp(argv[i], name)) return atoi(argv[i+1]);
	return def;
}

const char* loptions(char *argv[], const char *name, const char* def)
{
	int	i;
	for (i = 0; argv[i]; i++) if (!strcmp(argv[i], name)) return argv[i+1];
	return def;
}

bool isopt(char *argv[], const char *name)
{
	int	i;
	for (i = 0; argv[i]; i++) if (!strcmp(argv[i], name)) return true;
	return false;
}

int lopt_Spe(int i, char *argv[], const char *name, char* path)
{
//	int	i;
//	for (i = 0; argv[i]; i++){
        if (!strcmp(argv[i], name)){
            strcpy(path, argv[i+1]);
        
            if (argv[i+2] && !strcmp(argv[i+2], "--n"))
                return atoi(argv[i+3]);
            else
                return 1;
        }
//    }	
    
    return 0;
}

//------------------------REGISTRATION TO DISCOVERY SYSTEM
#include <stdio.h>      
#include <sys/types.h>
#include <ifaddrs.h>
#include <netinet/in.h> 
#include <string.h> 
#include <arpa/inet.h>
#include <netdb.h>

string searchIP()
{
    
    // Works also on OSX...
    
//#ifdef __linux__
	struct ifaddrs * ifAddrStruct = NULL;
    struct ifaddrs * ifa = NULL;
    void * tmpAddrPtr = NULL;
    string res = "127.0.0.1";
    
    getifaddrs(&ifAddrStruct);
    
    for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa ->ifa_addr->sa_family == AF_INET) { // check it is IP4
            // is a valid IP4 Address
            tmpAddrPtr = &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
            char addressBuffer[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
            printf("%s IP Address %s\n", ifa->ifa_name, addressBuffer);
			if (strcmp(addressBuffer, "127.0.0.1") != 0) {
				res = addressBuffer;
                break;
            }
		}  
    }
    if (ifAddrStruct != NULL) freeifaddrs(ifAddrStruct);
    return res;
/*
#else
    char host_name[256];
    gethostname(host_name, sizeof(host_name));
    struct hostent* host = gethostbyname(host_name);
    string res = "127.0.0.1";
    
    if (host){
        for (int i=0; host->h_addr_list[i] != 0; i++){
            struct in_addr addr;
            memcpy(&addr, host->h_addr_list[i], sizeof(struct in_addr));
            if (strcmp(inet_ntoa(addr), "127.0.0.1") != 0) {
				res = inet_ntoa(addr);
                break;
            }
        }
    }
    return res;
#endif
 */
}

