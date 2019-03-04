
#ifndef __UTILS_H
#define __UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <crypt.h>

//Constants
#define SALT_SIZE (3)
#define TAG_SIZE (100)
#define PASSWORD_SIZE (20)

#define MAX_TASKS (30)
#define MAX_PASSWORDS (30)

#define N_PASSWORDS (NTASKS)

#define MAX_RAND (99999999)

//Random number generation
#define GET_RANDOM (rand()) 
#define GET_RANDOM_IN_BOUNDS(a,b)  ( (a) + ( GET_RANDOM % ((b)-(a)) ) )

#define GET_RANDMON_SALT(salt)                                                              \
    do{                                                                                     \
        sprintf(salt,"%c%c",GET_RANDOM_IN_BOUNDS('a','z'),GET_RANDOM_IN_BOUNDS('a','z'));   \
    }while(0)

#define GET_RANDOM_STR_IN_BOUNDS(str,a,b)				\
	do{													\
		sprintf(str,"%08d",GET_RANDOM_IN_BOUNDS(a,b));	\
	}while(0)


//Utils
#define IS_EQUAL_TO_STRING(str1,str2) (strcmp(str1,str2)==0)

#define ENCRYPT(src, dest, salt)    \
    do{                             \
        char * tag;                 \
        strcpy(dest,src);           \
        tag = crypt(dest,salt);     \
        strcpy(dest,tag);           \
    }while(0)

#define LOG(str, ...)                           \
    do{                                         \
        fprintf(stderr, str, ##__VA_ARGS__);    \
        fflush(stderr);                         \
    }while(0)

//Delete when finished
#define DEBUG_LINE LOG("\n[%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);

#endif