
#ifndef __HEADER_H
#define __HEADER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <crypt.h>
#include <time.h>
#include <mpi.h>


//Constants
#define TRUE 1
#define FALSE 0

#define PASSWORD_SIZE (20)
#define SALT_SIZE (3)

#define MAX_TASKS (4)

#define TAG_SIZE 100

//Data definition
typedef unsigned short bool;
typedef char Salt [SALT_SIZE];
typedef int PasswordID, TaskID;
typedef enum{DECODE_REQUEST=10,DECODE_RESPONSE=11,DECODE_STOP=12,FINALIZE=13,UNKNOWN=14} MessageTag;

//NOTA
//
//      SI ALGUIEN TOCA EL ORDEN, NOMBRE, POSICION, .... DE ALGUNA ESTRUCTURA, LO MATO
//          SI ALGUIEN CAMBIA ALGO DE LAS ESTRUCTURAS SIN CAMBIARLO LO MATO
//                  SI CAMBIAS ALGO DISELO A FRAN, O SUFRIRAS LAS CONSECUENCIAS

typedef struct{
    Salt s;
    PasswordID passwordId;
	char decrypted[PASSWORD_SIZE];
	char encrypted[PASSWORD_SIZE];
}Password;

typedef struct{
    //here goes the range to find random numbers if neccesary
    bool finished;
    Password p;
}Request;

typedef struct{
	int ntries;
	Password p;
    TaskID taskId;
}Response;

//created only to associate which task is doing which request
typedef struct{
    TaskID taskId;
    PasswordID passwordId;
}Work;

//define error messages
#define USAGE_ERROR "./decrypt <encripted data (size = 9)>"

//Id management
#define ID ( getId() )
#define MASTER_ID ( 0 )
#define IS_MASTER(id) ( (id) == (MASTER_ID) )

//task management
#define NTASKS ( getNtasks() )

//data types
#define MPI_REQUEST_STRUCT (getMPI_REQUEST_STRUCT())
#define MPI_RESPONSE_STRUCT (getMPI_RESPONSE_STRUCT())
#define MPI_PASSWORD_STRUCT (getMPI_PASSWORD_STRUCT())

//Random number generation
#define MAX_RAND ( 100000000 )
#define GET_RANDOM ( rand() ) 
#define GET_RANDOM_IN_BOUNDS(a,b)  ( (a) + ( GET_RANDOM % ((b)-(a)) ) )

#define GET_RANDMON_SALT(salt)                                                              \
    do{                                                                                     \
        sprintf(salt,"%c%c",GET_RANDOM_IN_BOUNDS('a','z'),GET_RANDOM_IN_BOUNDS('a','z'));   \
    }while(0)

#define GET_RANDOM_STR_IN_BOUNDS(str,a,b)				\
	do{													\
		sprintf(str,"%08d",GET_RANDOM_IN_BOUNDS(a,b));	\
	}while(0)

#define ENCRYPT(src, dest, salt)    \
    do{                             \
        char * tag;                 \
        strcpy(dest,src);           \
        tag = crypt(dest,salt);     \
        strcpy(dest,tag);           \
    }while(0)

//Error and log management
#define MPI_EXIT(code)                     \
    do{	                                   \
		LOG("\n[ID:%d][Finalized]",ID);    \
    	MPI_Finalize();                    \
    	exit(code);                        \
    }while(0)

#define LOG(str, ...)                           \
    do{                                         \
        fprintf(stderr, str, ##__VA_ARGS__);    \
        fflush(stderr);							\
    }while(0)

#define EXIT_ON_FAILURE(returnValue)                                                                                \
    do{                                                                                                             \
        int tagsize;                                                                                                \
        char errortag[TAG_SIZE];                                                                                    \
        int code = (returnValue);                                                                                   \
        if(code != (MPI_SUCCESS)){                                                                                  \
            MPI_Error_string(code, errortag, &tagsize);                                                             \
            LOG("\n[ID %d][ERROR %d][%s:%d:%s] %s", ID, code, __FILE__, __LINE__, __FUNCTION__, errortag);     \
            MPI_EXIT(EXIT_FAILURE);                                                                                 \
        }                                                                                                           \
    }while(0)

//Utils
#define IS_EQUAL_TO_STRING(str1,str2) (strcmp(str1,str2)==0)

//Delete when finished
#define DEBUG_LINE LOG("\n[%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);

#endif