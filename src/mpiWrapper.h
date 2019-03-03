#include "header.h"

#ifndef __MPIWRAPPER_H
#define __MPIWRAPPER_H

//Wrapping and treatment of MPI library
int getId();
int getNtasks();

MPI_Datatype getMPI_PASSWORD_STRUCT();
MPI_Datatype getMPI_REQUEST_STRUCT();
MPI_Datatype getMPI_RESPONSE_STRUCT();

bool areThereAnyMsg();
void send(TaskID destinationAddr, void * data, MPI_Datatype tipo_datos, MessageTag tag);
void recv(TaskID destinationAddr, void * data, MPI_Datatype tipo_datos, MessageTag tag);

//auxiliar
char * passwordToString(Password p);
char * requestToString(Request req);
char * responseToString(Response res);
char * messageTagToSring(MessageTag msg);

#endif