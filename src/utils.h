
#ifndef __UTILS_H
#define __UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <crypt.h>
#include <time.h>
#include <unistd.h>
#include <mpi.h>

//Constants
#define SALT_SIZE (3)
#define TAG_SIZE (100)
#define PASSWORD_SIZE (20)
#define MAX_TASKS (30)
#define MAX_PASSWORDS (30)


#define N_TASKS (MPITASKS)  //defined at the makefile
#define N_PASSWORDS (20)     //defined here locally

#define NUMCHECKSMAIL (10)
#define SEED_BASE (3)
#define MAX_RAND (9999999)

//Random number generation
#define GET_SEED(id) ( (id)+(SEED_BASE) )
#define GET_RANDOM (rand()) 
#define GET_RANDOM_IN_BOUNDS(a,b)  ( (a) + ( GET_RANDOM % ((b)-(a)) ) )

#define GET_RANDMON_SALT(salt)                                                      \
    do{                                                                             \
        int n1 = GET_RANDOM_IN_BOUNDS(0,61);                                        \
        int n2 = GET_RANDOM_IN_BOUNDS(0,61);                                        \
        char c1 = (n1 < 10) ? (n1 + 48) : ( (n1 < 36) ? (n1 + 55) : ( n1 + 61 ) );  \
        char c2 = (n2 < 10) ? (n2 + 48) : ( (n2 < 36) ? (n2 + 55) : ( n2 + 61 ) );  \
        sprintf(salt,"%c%c",c1,c2);                                                 \
    }while(0)

#define GET_RANDOM_STR_IN_BOUNDS(str,a,b)				\
	do{													\
		sprintf(str,"%08d",GET_RANDOM_IN_BOUNDS(a,b));	\
	}while(0)

//mpi facilities
#define ID ( getId() )
#define MPITASKS ( getNtasks() )
#define PROC_NAME ( getProcName() )

#define MASTER_ID ( 0 )
#define IS_MASTER(id) ( (id) == (MASTER_ID) )

#define NULL_DATATYPE MPI_INT
#define MPI_REQUEST_STRUCT(request) (getMPI_REQUEST_STRUCT(request))
#define MPI_RESPONSE_STRUCT(response) (getMPI_RESPONSE_STRUCT(response))
#define MPI_PASSWORD_STRUCT(password) (getMPI_PASSWORD_STRUCT(password))

//Utils
#define NO_TASKS_WORKING_IN_PASSWORD(ntasks) ((ntasks)==0)
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

    
#define EXIT(code)       \
    do{                  \
        MPI_Finalize();  \
        exit(code);      \
    }while(0)



//data definition

#define TRUE 1
#define FALSE 0

typedef unsigned short bool;
typedef char Salt [SALT_SIZE];
typedef int PasswordID, TaskID;
typedef enum{DECODE_REQUEST=10,DECODE_RESPONSE=11,DECODE_STOP=12,FINALIZE=13,FINALIZE_RESPONSE=14,IDENTITY=15} MessageTag;

typedef struct{
    Salt s;
    PasswordID id;
    char decrypted[PASSWORD_SIZE];
    char encrypted[PASSWORD_SIZE];
}Password;

typedef struct{
    int rangeMin;
    int rangeMax;
    Password p;
}Request;

typedef struct{
    long ntries;
    double time;
    TaskID taskId;
    PasswordID passwordId;;
}Response;

typedef struct{
    bool finished;
    int numTasksDecrypting;

    PasswordID passwordId;
    TaskID taskIds[MAX_TASKS];

    Response solverResponse;
}PasswordStatus;


#endif