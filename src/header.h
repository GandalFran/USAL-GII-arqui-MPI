
#ifndef __HEADER_H
#define __HEADER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <crypt.h>
#include <mpi.h>


//Constants
#define TRUE 1
#define FALSE 0

#define PASSWORD_SIZE (20)
#define SALT_SIZE (2)

#define N_PASSWORDS (4)

//Data definition
typedef unsigned short bool;

typedef char Salt [SALT_SIZE];
typedef char * SaltPointer;

typedef struct{
	char decrypted[PASSWORD_SIZE];
	char encrypted[PASSWORD_SIZE];
}Password;

typedef struct{
	int id;
	int ntries;
	Password p;
}Solution;

typedef enum { MASTER_MODE, CALCULATOR_MODE } CalculationMode;

//define error messages
#define USAGE_ERROR "./decrypt <encripted data (size = 9)>"

//Id management
#define ID ( getId() )
#define MASTER_ID ( 0 )
#define IS_MASTER(id) ( (id) == (MASTER_ID) )

//Random number generation
#define MAX_RAND ( 100000000 )
#define GET_RANDOM ( rand() ) 
#define GET_RANDOM_IN_BOUNDS(a,b)  ( (a) + ( GET_RANDOM % ((b)-(a)) ) )

//NOTE: ITS NEED TO GENERATE THE SALT, HERE IS HARDCODED
#define GET_RANDMON_SALT(salt) strcpy(salt,"aa");

#define GET_RANDOM_STR_IN_BOUNDS(str,a,b)				\
	do{													\
		sprintf(str,"%08d",GET_RANDOM_IN_BOUNDS(a,b));	\
	}while(0)

//Error and log management
#define MPI_EXIT(code) 				\
    do{								\
		LOG("\n[ID:%d] Ended",ID);	\
    	MPI_Finalize();				\
    	exit(code);					\
    }while(0)

#define LOG(str, ...)                           \
    do{                                         \
        fprintf(stderr, str, ##__VA_ARGS__);	\
        fflush(stderr);							\
    }while(0)

#define EXIT_ON_WRONG_VALUE(wrongValue,returnValue,str, ...)  \
    do{                                         \
        if((returnValue) == (wrongValue)){      \
            LOG(str,##__VA_ARGS__);             \
            MPI_EXIT(EXIT_FAILURE);             \
        }                                       \
    }while(0)


//Utils
#define IS_EQUAL_TO_STRING(str1,str2) (strcmp(str1,str2)==0)

//Delete when finished
#define DEBUG_LINE LOG("\n[%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);

#endif