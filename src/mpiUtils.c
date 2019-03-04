#include "mpiUtils.h"

//Wrap to only get the Id one time
int getId(){
	static int internalId = -1;

	if(-1 == internalId)
		MPI_Comm_rank(MPI_COMM_WORLD, &internalId);
	
	return internalId;
}

//Wrap to get the number of task one time
int getNtasks(){
	static int ntasks = -1;

	if(-1 == ntasks)
		MPI_Comm_size(MPI_COMM_WORLD, &ntasks);
	
	return ntasks;
}

//Wrap the send, recv, ... and more communication tasks
bool areThereAnyMsg(){
	int result;
	MPI_Status status;
	EXIT_ON_FAILURE(MPI_Iprobe( MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &result, &status));
	return (result != 0) ? TRUE : FALSE;
}

void send(TaskID destinationAddr, void * data, MPI_Datatype tipo_datos, MessageTag tag){

	if(NULL != data)
		EXIT_ON_FAILURE(MPI_Send(data, 1, tipo_datos, destinationAddr, tag, MPI_COMM_WORLD));
	else{
		int foo = 1;
		EXIT_ON_FAILURE(MPI_Send(&foo, 1, MPI_INT, destinationAddr, tag, MPI_COMM_WORLD));
		//EXIT_ON_FAILURE(MPI_Send(NULL, 1, MPI_DATATYPE_NULL, destinationAddr, tag, MPI_COMM_WORLD));
	}
/*	switch(tag){
		case DECODE_REQUEST: 	LOG("\n[ID %d][send] %s to %d content %s", ID, messageTagToSring(tag), destinationAddr,requestToString(*(Request *)data));		break;
		case DECODE_RESPONSE: 	LOG("\n[ID %d][send] %s to %d content %s", ID, messageTagToSring(tag), destinationAddr,responseToString(*(Response *)data));	break;
		//case DECODE_STOP: 		LOG("\n[ID %d][send] %s to %d content NULL", ID, messageTagToSring(tag), destinationAddr); break;
		case FINALIZE: 			LOG("\n[ID %d][send] %s to %d content NULL", ID, messageTagToSring(tag), destinationAddr); break;
		default: LOG("\n[ID %d][send] %s to %d ", ID, messageTagToSring(tag), destinationAddr);
	}*/

}

void recv(TaskID destinationAddr, void * data, MPI_Datatype tipo_datos, MessageTag tag){
	MPI_Status status;
	int foo;

	if(NULL != data)
		EXIT_ON_FAILURE(MPI_Recv(data, 1, tipo_datos, destinationAddr, tag, MPI_COMM_WORLD, &status));
	else{
		EXIT_ON_FAILURE(MPI_Recv(&foo, 1, MPI_INT, destinationAddr, tag, MPI_COMM_WORLD, &status));
		//EXIT_ON_FAILURE(MPI_Recv(NULL, 1, MPI_DATATYPE_NULL, destinationAddr, tag, MPI_COMM_WORLD, &status));
	}
/*
	switch(status.MPI_TAG){
		case DECODE_REQUEST: 	LOG("\n[ID %d][recv] %s from %d content %s", ID, messageTagToSring(status.MPI_TAG), status.MPI_SOURCE,requestToString(*(Request *)data));	break;
		case DECODE_RESPONSE: 	LOG("\n[ID %d][recv] %s from %d content %s", ID, messageTagToSring(status.MPI_TAG), status.MPI_SOURCE,responseToString(*(Response *)data));	break;
		//case DECODE_STOP: 		LOG("\n[ID %d][recv] %s from %d content NULL", ID, messageTagToSring(status.MPI_TAG), status.MPI_SOURCE); break;
		case FINALIZE: 			LOG("\n[ID %d][recv] %s from %d content NULL", ID, messageTagToSring(status.MPI_TAG), status.MPI_SOURCE); break;
		default: LOG("\n[ID %d][recv] %s from %d ", ID, messageTagToSring(status.MPI_TAG), status.MPI_SOURCE);
	}*/

	if(status.MPI_TAG == FINALIZE)
		MPI_EXIT(EXIT_SUCCESS);
}

MPI_Datatype getMPI_PASSWORD_STRUCT(Password p){
	MPI_Datatype dataType;
	int size[4];
	MPI_Aint shift[4];
	MPI_Aint address[5];
	MPI_Datatype types[4];

	types[0] = MPI_CHAR;
	types[1] = MPI_INT; 
	types[2] = MPI_CHAR;
	types[3] = MPI_CHAR;

	size[0] = SALT_SIZE;
	size[1] = 1;
	size[2] = PASSWORD_SIZE;
	size[3] = PASSWORD_SIZE;

	MPI_Address(&p,&address[0]);
	MPI_Address(&(p.s),&address[1]);
	MPI_Address(&(p.passwordId),&address[2]);
	MPI_Address(&(p.decrypted),&address[3]);
	MPI_Address(&(p.encrypted),&address[4]);
		
	shift[0] = address[1] - address[0];
	shift[1] = address[2] - address[0];
	shift[2] = address[3] - address[0];
	shift[3] = address[4] - address[0];

	MPI_Type_struct(4,size,shift,types,&dataType);
	MPI_Type_commit(&dataType);

	return dataType;
}

MPI_Datatype getMPI_REQUEST_STRUCT(Request req){
	MPI_Datatype dataType;
	int size[3];
	MPI_Aint shift[3];
	MPI_Aint address[4];
	MPI_Datatype types[3];

	types[0] = MPI_INT; 
	types[1] = MPI_INT; 
	types[2] = MPI_PASSWORD_STRUCT(req.p);

	size[0] = 1;
	size[1] = 1;
	size[2] = 1;

	MPI_Address(&req,&address[0]);
	MPI_Address(&(req.rangeMin),&address[1]);
	MPI_Address(&(req.rangeMax),&address[2]);
	MPI_Address(&(req.p),&address[3]);
		
	shift[0] = address[1] - address[0];
	shift[1] = address[2] - address[0];
	shift[2] = address[3] - address[0];

	MPI_Type_struct(3,size,shift,types,&dataType);
	MPI_Type_commit(&dataType);

	return dataType;
}

MPI_Datatype getMPI_RESPONSE_STRUCT(Response res){
	MPI_Datatype dataType;
	int size[3];
	MPI_Aint shift[3];
	MPI_Aint address[4];
	MPI_Datatype types[3];

	types[0] = MPI_INT; 
	types[1] = MPI_INT;
	types[2] = MPI_PASSWORD_STRUCT(res.p);

	size[0] = 1;
	size[1] = 1;
	size[2] = 1;

	MPI_Address(&res,&address[0]);
	MPI_Address(&(res.ntries),&address[1]);
	MPI_Address(&(res.taskId),&address[2]);
	MPI_Address(&(res.p),&address[3]);
		
	shift[0] = address[1] - address[0];
	shift[1] = address[2] - address[0];
	shift[2] = address[3] - address[0];

	MPI_Type_struct(3,size,shift,types,&dataType);
	MPI_Type_commit(&dataType);

	return dataType;
}