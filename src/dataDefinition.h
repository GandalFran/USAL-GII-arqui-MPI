
#ifndef __DATATYPES_H
#define __DATATYPES_H

#include <stdlib.h>
#include "utils.h"

#define TRUE 1
#define FALSE 0

typedef unsigned short bool;
typedef char Salt [SALT_SIZE];
typedef int PasswordID, TaskID;
typedef enum{DECODE_REQUEST=10,DECODE_RESPONSE=11,FINALIZE=13,UNKNOWN=14} MessageTag;

typedef struct{
    Salt s;
    PasswordID passwordId;
	char decrypted[PASSWORD_SIZE];
	char encrypted[PASSWORD_SIZE];
}Password;

typedef struct{
    int rangeMin;
    int rangeMax;
    Password p;
}Request;

typedef struct{
	int ntries;
    long time;
    TaskID taskId;
    Password p;
}Response;

typedef struct{
    bool finished;
    PasswordID passwordId;
    int numTasksDecrypting;
    TaskID taskIds[MAX_TASKS];
}PasswordStatus;
#endif