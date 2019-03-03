#include "mpiWrapper.h"

// -------------------------------- MPI --------------------------------

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

LOG("\n[ID %d]checking mail", ID);
	EXIT_ON_FAILURE(MPI_Iprobe( MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &result, &status));

	if(result != 0)
		LOG("\n[ID %d][mail check] yes from:%d type:%s",ID,status.MPI_SOURCE,messageTagToSring(status.MPI_TAG));
	else
		LOG("\n[ID %d][mail check] no ",ID);


	return (result != 0) ? TRUE : FALSE;
}

void send(TaskID destinationAddr, void * data, MPI_Datatype tipo_datos, MessageTag tag){
	LOG("\n[ID %d][send] %s to %d ", ID, messageTagToSring(tag), destinationAddr);
	EXIT_ON_FAILURE(MPI_Send(data, (data == NULL) ? 0 : 1, tipo_datos, destinationAddr, tag, MPI_COMM_WORLD));
}

void recv(TaskID destinationAddr, void * data, MPI_Datatype tipo_datos, MessageTag tag){
	MPI_Status status;
	EXIT_ON_FAILURE(MPI_Recv(data,  (data == NULL) ? 0 : 1, tipo_datos, destinationAddr, tag, MPI_COMM_WORLD, &status));
	LOG("\n[ID %d][recv] %s from %d", ID, messageTagToSring(status.MPI_TAG), status.MPI_SOURCE);
}

//define the data types
MPI_Datatype getMPI_PASSWORD_STRUCT(){
	static bool firstBuild = FALSE;
	static MPI_Datatype dataType;
	
	Password p;
	int size[4];
	MPI_Aint shift[4];
	MPI_Aint address[5];
	MPI_Datatype types[4];

	if(!firstBuild){

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

		firstBuild = TRUE;
	}

	return dataType;
}

MPI_Datatype getMPI_REQUEST_STRUCT(){
	static bool firstBuild = FALSE;
	static MPI_Datatype dataType;
	
	Request req;
	int size[2];
	MPI_Aint shift[2];
	MPI_Aint address[3];
	MPI_Datatype types[2];

	if(!firstBuild){

		types[0] = MPI_UNSIGNED_SHORT; 
		types[1] = MPI_PASSWORD_STRUCT;

		size[0] = 1;
		size[1] = 1;

		MPI_Address(&req,&address[0]);
		MPI_Address(&(req.finished),&address[1]);
		MPI_Address(&(req.p),&address[2]);
		
		shift[0] = address[1] - address[0];
		shift[1] = address[2] - address[0];

		MPI_Type_struct(2,size,shift,types,&dataType);
		MPI_Type_commit(&dataType);

		firstBuild = TRUE;
	}

	return dataType;
}

MPI_Datatype getMPI_RESPONSE_STRUCT(){
	static bool firstBuild = FALSE;
	static MPI_Datatype dataType;
	
	Response res;
	int size[3];
	MPI_Aint shift[3];
	MPI_Aint address[4];
	MPI_Datatype types[3];

	if(!firstBuild){

		types[0] = MPI_INT; 
		types[1] = MPI_PASSWORD_STRUCT;
		types[2] = MPI_INT;

		size[0] = 1;
		size[1] = 1;
		size[2] = 1;

		MPI_Address(&res,&address[0]);
		MPI_Address(&(res.ntries),&address[1]);
		MPI_Address(&(res.p),&address[2]);
		MPI_Address(&(res.taskId),&address[3]);
		
		shift[0] = address[1] - address[0];
		shift[1] = address[2] - address[0];
		shift[2] = address[3] - address[0];

		MPI_Type_struct(3,size,shift,types,&dataType);
		MPI_Type_commit(&dataType);

		firstBuild = TRUE;
	}

	return dataType;
}

// -------------------------------- Auxiliar --------------------------------

char * passwordToString(Password p){
	static char tag[TAG_SIZE];

	memset(tag,0,TAG_SIZE);
	sprintf(tag,"Password{id:%d, decrypted:%s, encrypted: %s, salt: %s}",p.passwordId,p.decrypted,p.encrypted,p.s);

	return tag;
}

char * requestToString(Request req){
	static char tag[TAG_SIZE];

	memset(tag,0,TAG_SIZE);
	sprintf(tag,"Request{finished:%d, password:%s }",req.finished,passwordToString(req.p));

	return tag;
}

char * responseToString(Response res){
	static char tag[TAG_SIZE];

	memset(tag,0,TAG_SIZE);
	sprintf(tag,"Response{id:%d, ntries: %d, password:%s }",res.taskId,res.ntries,passwordToString(res.p));

	return tag;
}

char * messageTagToSring(MessageTag msg){
	static char tag[TAG_SIZE];

	memset(tag,0,TAG_SIZE);
	switch(msg){
		case DECODE_REQUEST: 	strcpy(tag,"DECODE_REQUEST");	break;
		case DECODE_RESPONSE: 	strcpy(tag,"DECODE_RESPONSE");	break;
		case DECODE_STOP: 		strcpy(tag,"DECODE_STOP");		break;
		case FINALIZE: 			strcpy(tag,"FINALIZE");			break;
		default: sprintf(tag,"UNKNOWN:%d",msg);
	}

	return tag;
}