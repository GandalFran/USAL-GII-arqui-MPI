
#include "header.h"

//Task definition
void communicationTask();
void calculationTask();
void masterCalculationTask();

//Calculus definition
bool doCalculus(Password * p);

//task division
void doTaskDivision(Request * requestList, Work * taskSituation);

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

//debug
void logToFile(char * toLog);


// -------------------------------- Main --------------------------------

int main (int argc, char * argv[]){
	//give the random seed
	srand(time(NULL));

	//initialize the MPI
	MPI_Init(&argc, &argv);
	LOG("\n[ID:%d] Started",ID);

	//Taking each one to his role
	if(IS_MASTER(ID)){
		communicationTask();
	}else{
		calculationTask();
	}

	//Finalize execution
	MPI_EXIT(EXIT_SUCCESS);
}


// -------------------------------- Task definition --------------------------------
void communicationTask(){
	Response resTmp;
	int solvedTasks, i;

	//------------------
	//	To know the curren situation:
	//		+ which task is doing which password -> taskSituation[ID]
	//		+ wich passwords has been solved --> requestList[passwordId].finished
	//------------------
	Request requestList[MAX_TASKS];
	Work taskSituation[MAX_TASKS];

	//reset the control arrays
	memset(requestList,0,MAX_TASKS*sizeof(Request));
	memset(taskSituation,0,MAX_TASKS*sizeof(Work));

	//generate passwords
	for(i=0; i<NTASKS; i++){
		requestList[i].p.passwordId = i;

		GET_RANDMON_SALT(requestList[i].p.s);
		GET_RANDOM_STR_IN_BOUNDS(requestList[i].p.decrypted,0,MAX_RAND);
		ENCRYPT(requestList[i].p.decrypted, requestList[i].p.encrypted, requestList[i].p.s);
		
		LOG("\n[ID %d][Generated] %s",ID,passwordToString(requestList[i].p));
	}


	for(solvedTasks = 0; solvedTasks < NTASKS; solvedTasks++){
		//Division of tasks
		if(solvedTasks == 0)
			doTaskDivision(requestList,taskSituation);

		//go to the calculation -> there a REQUEST for calculus will be recived
		masterCalculationTask();

		recv(MPI_ANY_SOURCE, &resTmp, MPI_RESPONSE_STRUCT, DECODE_RESPONSE);

		//-----response gestion------
		LOG("\n[ID %d][Recived response] %s",ID,responseToString(resTmp));

		//mark password as solved
		requestList[resTmp.p.passwordId].finished = TRUE;

		//advise all tasks that are solving this password, to stop
		for(i =0; i< NTASKS; i++){
			if( taskSituation[i].passwordId == resTmp.p.passwordId ){
				//send a stop message
				send(i, NULL, MPI_BYTE , DECODE_STOP);
			}
		}

		//if neccesary save here the response, to do statistics before

	}

	//finalize all tasks
	for(i=1; i< NTASKS; i++){
		send(i, NULL , MPI_BYTE , FINALIZE);
	}

}

void calculationTask(){
	Request request;
	Response response;
	bool recivedFinalizeOrder = FALSE;

	//reset the response
	memset(&response,0,sizeof(Response));
	response.taskId = ID;

	do{
		//Reset the request and wait to password request or finalize
		memset(&request,0,sizeof(Request));
		recv(MASTER_ID, &request, MPI_REQUEST_STRUCT, DECODE_REQUEST);

		do{
			//increment the try number
			response.ntries++;

			//do the calculus and then check if the solution has been found
			if(doCalculus(&(request.p))){
				//fill the response
				memcpy(&(response.p),&(request.p),sizeof(Password));
				LOG("\n[ID %d][Solution found] %s",ID,responseToString(response));
				
				//send
				send(MASTER_ID, &response, MPI_RESPONSE_STRUCT, DECODE_RESPONSE);

				//go to the external loop
				break;
			}

			//Non-blocking check if Master has ordered to stop, or to finalize our calculation
			if(areThereAnyMsg()){
				//recv message to discard it --> it is only a order to finalize or change task
				//pbolem -> the message could be a finalize or a decode_stop, and the message is void
				recv(MASTER_ID, NULL, MPI_BYTE , MPI_ANY_TAG);
				break;
			}

		}while(TRUE);
	
	}while(!recivedFinalizeOrder);
}

void masterCalculationTask(){
	Request request;
	Response response;

	//reset the response
	memset(&response,0,sizeof(Response));
	response.taskId = ID;

	//Reset the request and wait to password request or finalize
	memset(&request,0,sizeof(Request));
	recv(MASTER_ID, &request, MPI_REQUEST_STRUCT, DECODE_REQUEST);

	do{
		//increment the try number
		response.ntries++;

		//do the calculus and then check if the solution has been found
		if(doCalculus(&(request.p))){
			//fill the response
			memcpy(&(response.p),&(request.p),sizeof(Password));
			LOG("\n[ID %d][Solution found] %s",ID,responseToString(response));

			//send
			send(MASTER_ID, &response, MPI_RESPONSE_STRUCT, DECODE_RESPONSE);

			//finalize the decoding 
			break;
		}

		//Check if a message has been recived --> its neccesary to handle a request an send
		if(areThereAnyMsg()){
			break;
		}

	}while(TRUE);
}

// -------------------------------- Calculus definition --------------------------------

bool doCalculus(Password * p){
	char possibleSolution[PASSWORD_SIZE], possibleSolutionEncripted[PASSWORD_SIZE];

	//generate and encrypt possible solution
	GET_RANDOM_STR_IN_BOUNDS(possibleSolution,0,MAX_RAND);
	ENCRYPT(possibleSolution, possibleSolutionEncripted, p->s);

	//LOG("\n[ID %d][Calculus] generated: %s generatedEncrypted: %s \n\toriginal: %s",ID,possibleSolution,possibleSolutionEncripted,passwordToString(*p));

	//check if is the possible solution is equal to the encripted data
	if ( IS_EQUAL_TO_STRING(possibleSolutionEncripted,p->encrypted) )
	{
		strcpy(p->decrypted,possibleSolution);
		return TRUE;
	}

	return FALSE;
}

// -------------------------------- Task division --------------------------------

void doTaskDivision(Request * requestList, Work * taskSituation){
	int i;

	//NOTE: for the moment, one task is sent to each agent 

	for(i=0; i<NTASKS; i++){
		if(!requestList[i].finished){
			//send message to task
			send(i, &requestList[i], MPI_REQUEST_STRUCT, DECODE_REQUEST);
			//register the work 
			taskSituation[i].taskId = i;
			taskSituation[i].passwordId = requestList[i].p.passwordId;
		}
	}

}

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

	MPI_Iprobe( MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &result, &status);
	//LOG("\n[ID %d][check mail] %s", ID, (result != 0) ? "yes mail" : "no mail" );

	return (result != 0) ? TRUE : FALSE;
}

void send(TaskID destinationAddr, void * data, MPI_Datatype tipo_datos, MessageTag tag){
	MPI_Request req;
	LOG("\n[ID %d][send] %s to %d ", ID, messageTagToSring(tag), destinationAddr);
	MPI_Isend(data, (data == NULL) ? 0 : 1, tipo_datos, destinationAddr, tag, MPI_COMM_WORLD, &req);
}

void recv(TaskID destinationAddr, void * data, MPI_Datatype tipo_datos, MessageTag tag){
	MPI_Status status;
	MPI_Recv(data,  (data == NULL) ? 0 : 1, tipo_datos, destinationAddr, tag, MPI_COMM_WORLD, &status);
	LOG("\n[ID %d][recv] %s from %d", ID, messageTagToSring(tag), destinationAddr);
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
		default: strcpy(tag,"UNKNOWN");
	}

	return tag;
}