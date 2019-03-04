#ifndef __MPIUTILS_H
#define __MPIUTILS_H

#include <mpi.h>
#include <stdlib.h>

#include "utils.h"
#include "dataDefinition.h"

#define ID ( getId() )
#define MPITASKS ( getNtasks() )

#define MASTER_ID ( 0 )
#define IS_MASTER(id) ( (id) == (MASTER_ID) )

#define MPI_REQUEST_STRUCT(request) (getMPI_REQUEST_STRUCT(request))
#define MPI_RESPONSE_STRUCT(response) (getMPI_RESPONSE_STRUCT(response))
#define MPI_PASSWORD_STRUCT(password) (getMPI_PASSWORD_STRUCT(password))
    
#define MPI_EXIT(code)                     \
    do{	                                   \
		LOG("\n[ID:%d][Finalized]",ID);    \
    	MPI_Finalize();                    \
    	exit(code);                        \
    }while(0)

#define EXIT_ON_FAILURE(returnValue)                                                                                \
    do{                                                                                                             \
        int tagsize;                                                                                                \
        char errortag[TAG_SIZE];                                                                                    \
        int code = (returnValue);                                                                                   \
        if(code != (MPI_SUCCESS)){                                                                                  \
            MPI_Error_string(code, errortag, &tagsize);                                                             \
            LOG("\n[ID %d][ERROR %d][%s:%d:%s] %s", ID, code, __FILE__, __LINE__, __FUNCTION__, errortag);     		\
            MPI_EXIT(EXIT_FAILURE);                                                                                 \
        }                                                                                                           \
    }while(0)

int getId();
int getNtasks();

bool areThereAnyMsg();
void send(TaskID destinationAddr, void * data, MPI_Datatype tipo_datos, MessageTag tag);
void recv(TaskID destinationAddr, void * data, MPI_Datatype tipo_datos, MessageTag tag);

MPI_Datatype getMPI_PASSWORD_STRUCT();
MPI_Datatype getMPI_REQUEST_STRUCT();
MPI_Datatype getMPI_RESPONSE_STRUCT();


#endif