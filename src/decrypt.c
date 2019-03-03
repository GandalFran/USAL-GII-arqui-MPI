
#include "header.h"

//Communication master behaviour
void masterCommunicationBehaviour();

void taskAssignation(TaskID task,Password * passwordList, PasswordStatus * passwordStatusList, Request * masterReq);
bool masterCalculationBehaviour(Request request, Response * response);
void responseGestion(PasswordStatus * passwordStatusList, Response * responseList, Response res);

//Calculation proces behaviour
void calculationBehaviour();

//Calculus definition
bool doCalculus(Password * p);

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
char * passwordStatusToString(PasswordStatus ps);
char * messageTagToSring(MessageTag msg);


// -------------------------------- Main --------------------------------

int main (int argc, char * argv[]){

	//initialize the MPI
	MPI_Init(&argc, &argv);
	MPI_Errhandler_set(MPI_COMM_WORLD, MPI_ERRORS_RETURN); 
	LOG("\n[ID:%d][%d] Started",ID,getpid());

	//give the random seed 
	srand(ID + time(NULL));

	//Taking each one to his role
	if(IS_MASTER(ID)){
		masterCommunicationBehaviour();
	}else{
		calculationBehaviour();
	}

	//Finalize execution
	MPI_EXIT(EXIT_SUCCESS);
}


// -------------------------------- Communication task behaviour --------------------------------
void masterCommunicationBehaviour(){
	bool solvedByMe;
	Response resTmp;
	Request reqToMaster;
	int solvedPasswords, i, lastSolvedPasswordId;

	Password passwordList[MAX_PASSWORDS];
	PasswordStatus passwordStatusList[MAX_PASSWORDS];
	Response responseList[MAX_PASSWORDS];

	//reset the control arrays and other vars
	memset(passwordList,0,MAX_PASSWORDS*sizeof(Password));
	memset(passwordStatusList,0,MAX_PASSWORDS*sizeof(PasswordStatus));
	memset(responseList,0,MAX_PASSWORDS*sizeof(Response));

	for(i=0; i<MAX_PASSWORDS; i++){
		passwordStatusList[i].lastAssigned = -1;
	}

	solvedByMe = FALSE;
	lastSolvedPasswordId = -1;

	//generate passwords
	for(i=0; i<N_PASSWORDS; i++){
		passwordList[i].passwordId = i;

		GET_RANDMON_SALT(passwordList[i].s);
		GET_RANDOM_STR_IN_BOUNDS(passwordList[i].decrypted,0,MAX_RAND);
		ENCRYPT(passwordList[i].decrypted, passwordList[i].encrypted, passwordList[i].s);
		
		LOG("\n[ID %d][Generated] %s",ID,passwordToString(passwordList[i]));
	}

	//wait for responses, and reasignate tasks to each free 
	for(solvedPasswords = 0; solvedPasswords < N_PASSWORDS; solvedPasswords++){
		//division of tasks: if is the firstTime, the asignation is to all tasks, if not, is to the tasks implicated in the last solved password
		if(-1 == lastSolvedPasswordId){
			for(i=0; i<NTASKS; i++){
				taskAssignation(i,passwordList,passwordStatusList, &reqToMaster);
			}
		}else{
			for(i=0; i<=passwordStatusList[lastSolvedPasswordId].lastAssigned; i++){
				taskAssignation(passwordStatusList[lastSolvedPasswordId].taskIds[i],passwordList,passwordStatusList, &reqToMaster);
			}
		}
LOG("\nEnding task assignation");
		//calculate the master assignement
		solvedByMe = masterCalculationBehaviour(reqToMaster,&resTmp);
LOG("\nEnding calculation");
		//wait for a response
		if(!solvedByMe)
			recv(MPI_ANY_SOURCE, &resTmp, MPI_RESPONSE_STRUCT, DECODE_RESPONSE);

		//make the response treatment
		responseGestion(passwordStatusList,responseList,resTmp);
LOG("\nEnding response gestion");
		//save the last solved password id to assign tasks in the next iteration
		lastSolvedPasswordId = resTmp.p.passwordId;
	}

	//finalize all tasks
	for(i=1; i< NTASKS; i++){
		send(i, NULL , MPI_DATATYPE_NULL , FINALIZE);
	}

}

void taskAssignation(TaskID task,Password * passwordList, PasswordStatus * passwordStatusList, Request * masterReq){
	Request req;
	int i, j, rangeIncrement;

	//check if a password is not assigned, and in that case, the password is assigned
	for(i=0; i< N_PASSWORDS; i++){
		//the initial value of lastAssigned is -1, because there are no processes decoding
		if(-1 == passwordStatusList[i].lastAssigned){
			//fill the request structure
			masterReq->rangeMin = 0;
			masterReq->rangeMax = MAX_RAND;
			memcpy(&(masterReq->p),&(passwordList[i]), sizeof(Password));
			memset(masterReq->p.decrypted,0,PASSWORD_SIZE);

			//send the message to task if is not for master
			if( !IS_MASTER(task) )
				send(task, masterReq, MPI_REQUEST_STRUCT, DECODE_REQUEST);

			//save the new password status
			passwordStatusList[i].lastAssigned++;
			passwordStatusList[i].taskIds[passwordStatusList[i].lastAssigned] = task;

			LOG("\n[ID %d] ID %d -> %s", ID, task, requestToString(*masterReq));
			return;
		}
	}

	//if all tasks are assigned, then is assigned the first task not solved
	for(i=0; i< N_PASSWORDS; i++){
		if(FALSE == passwordStatusList[i].finished){

			//foreach implicated task, send a stop 
			for(j=0; j<=passwordStatusList[i].lastAssigned; j++){
				if( !IS_MASTER( passwordStatusList[i].taskIds[j]) ){
					send( passwordStatusList[i].taskIds[j], NULL, MPI_DATATYPE_NULL, DECODE_STOP);
				}
			}

			//save the new password status
			passwordStatusList[i].lastAssigned++;
			passwordStatusList[i].taskIds[passwordStatusList[i].lastAssigned] = task;

			//calculate the range increments
			rangeIncrement = (int) MAX_RAND / passwordStatusList[i].lastAssigned;

			//fill the request structure
			req.rangeMin = req.rangeMax = 0;
			memcpy(&(req.p),&(passwordList[i]), sizeof(Password));
			memset(req.p.decrypted,0,PASSWORD_SIZE);

			//Send the new requests
			for(j=0; j<=passwordStatusList[i].lastAssigned; j++){

				//refresh the ranges -> NOTE:if is the last assignement, the range limit is the maximum 
				req.rangeMin = req.rangeMax + 1;
				if(j == passwordStatusList[i].lastAssigned)
					req.rangeMax = MAX_RAND;
				else
					req.rangeMax = req.rangeMin + rangeIncrement;

				//send the request
				if( IS_MASTER( passwordStatusList[i].taskIds[j]))
					memcpy(masterReq,&req,sizeof(Request));
				else
					send( passwordStatusList[i].taskIds[j], &req, MPI_REQUEST_STRUCT, DECODE_REQUEST);

				LOG("\n[ID %d] ID %d -> %s", ID, passwordStatusList[i].taskIds[j], requestToString(req));
			}

			return;
		}
	}

	LOG("\nTROUBLE: the main task entered here but all tasks are assigned");
}

void responseGestion(PasswordStatus * passwordStatusList, Response * responseList, Response res){
	int i;
	PasswordID passwordId = res.p.passwordId;

	//mark password as solved
	passwordStatusList[passwordId].finished = TRUE;

	//advise all tasks that are solving this password, to stop
	for(i =0; i<= passwordStatusList[passwordId].lastAssigned; i++){
		if( !IS_MASTER( passwordStatusList[passwordId].taskIds[i]) && passwordStatusList[passwordId].taskIds[i] != res.taskId){
			//send a stop message to stop all tasks, except the one which has solved the request 
			send(i, NULL, MPI_DATATYPE_NULL , DECODE_STOP);
		}
	}
	
	//mark the task as solved and save the response
	passwordStatusList[passwordId].finished = TRUE;
	memcpy(&(responseList[passwordId]),&res,sizeof(Response));
}

bool masterCalculationBehaviour(Request request, Response * response){
	
	//reset the response
	memset(response,0,sizeof(Response));
	response->taskId = ID;

	do{
		//increment the try number
		(response->ntries)++;

		//do the calculus and then check if the solution has been found
		if(doCalculus(&(request.p))){
			//fill the response
			memcpy(&(response->p),&(request.p),sizeof(Password));
			//return TRUE, to know before, that the master has finished the task
			return TRUE;
		}

		//Check if a message has been recived --> its neccesary to handle a request an send
		if(areThereAnyMsg()){
			return FALSE;
		}

	}while(TRUE);
}

// -------------------------------- Usual processes behaviour --------------------------------

void calculationBehaviour(){
	Request request;
	Response response;
	bool recivedFinalizeOrder = FALSE;

	//reset the response
	memset(&response,0,sizeof(Response));
	response.taskId = ID;

	do{
		//Reset the request and wait to password request or finalize
		memset(&request,0,sizeof(Request));
		recv(MASTER_ID, &request, MPI_REQUEST_STRUCT, MPI_ANY_TAG);

		do{
			//increment the try number
			response.ntries++;

			//do the calculus and then check if the solution has been found
			if(doCalculus(&(request.p))){
				//fill the response and send to master
				memcpy(&(response.p),&(request.p),sizeof(Password));
				send(MASTER_ID, &response, MPI_RESPONSE_STRUCT, DECODE_RESPONSE);

				//go to the external loop
				LOG("\n[ID %d][Solution found] %s",ID,responseToString(response));
				break;
			}

			//Non-blocking check if Master has ordered to stop, or to finalize our calculation
			if(areThereAnyMsg()){
				//recv message to discard it --> it is only a order to finalize or change task
				recv(MASTER_ID, NULL, MPI_DATATYPE_NULL , MPI_ANY_TAG);
				break;
			}

		}while(TRUE);
	
	}while(!recivedFinalizeOrder);
}

// -------------------------------- Calculus definition --------------------------------

bool doCalculus(Password * p){
	char possibleSolution[PASSWORD_SIZE], possibleSolutionEncripted[PASSWORD_SIZE];

	//generate and encrypt possible solution
	GET_RANDOM_STR_IN_BOUNDS(possibleSolution,0,MAX_RAND);
	ENCRYPT(possibleSolution, possibleSolutionEncripted, p->s);

	//check if is the possible solution is equal to the encripted data
	if ( IS_EQUAL_TO_STRING(possibleSolutionEncripted,p->encrypted) )
	{
		strcpy(p->decrypted,possibleSolution);
		return TRUE;
	}
	return FALSE;
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
	EXIT_ON_FAILURE(MPI_Iprobe( MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &result, &status));
	return (result != 0) ? TRUE : FALSE;
}

void send(TaskID destinationAddr, void * data, MPI_Datatype tipo_datos, MessageTag tag){

	if(NULL != data)
		EXIT_ON_FAILURE(MPI_Send(data, 1, tipo_datos, destinationAddr, tag, MPI_COMM_WORLD));
	else{
		int foo;
		EXIT_ON_FAILURE(MPI_Send(&foo, 1, MPI_INT, destinationAddr, tag, MPI_COMM_WORLD));
	}

	/*switch(tag){
		case DECODE_REQUEST: 	LOG("\n[ID %d][send] %s to %d content %s", ID, messageTagToSring(tag), destinationAddr,requestToString(*(Request *)data));		break;
		case DECODE_RESPONSE: 	LOG("\n[ID %d][send] %s to %d content %s", ID, messageTagToSring(tag), destinationAddr,responseToString(*(Response *)data));	break;
		case DECODE_STOP: 		LOG("\n[ID %d][send] %s to %d content NULL", ID, messageTagToSring(tag), destinationAddr); break;
		case FINALIZE: 			LOG("\n[ID %d][send] %s to %d content NULL", ID, messageTagToSring(tag), destinationAddr); break;
		default: LOG("\n[ID %d][send] %s to %d ", ID, messageTagToSring(tag), destinationAddr);
	}*/

}

void recv(TaskID destinationAddr, void * data, MPI_Datatype tipo_datos, MessageTag tag){
	MPI_Status status;

	if(NULL != data)
		EXIT_ON_FAILURE(MPI_Recv(data, 1, tipo_datos, destinationAddr, tag, MPI_COMM_WORLD, &status));
	else{
		int foo;
		EXIT_ON_FAILURE(MPI_Recv(&foo, 1, MPI_INT, destinationAddr, tag, MPI_COMM_WORLD, &status));
	}

	/*switch(status.MPI_TAG){
		case DECODE_REQUEST: 	LOG("\n[ID %d][recv] %s from %d content %s", ID, messageTagToSring(status.MPI_TAG), status.MPI_SOURCE,requestToString(*(Request *)data));	break;
		case DECODE_RESPONSE: 	LOG("\n[ID %d][recv] %s from %d content %s", ID, messageTagToSring(status.MPI_TAG), status.MPI_SOURCE,responseToString(*(Response *)data));	break;
		case DECODE_STOP: 		LOG("\n[ID %d][recv] %s from %d content NULL", ID, messageTagToSring(status.MPI_TAG), status.MPI_SOURCE); break;
		case FINALIZE: 			LOG("\n[ID %d][recv] %s from %d content NULL", ID, messageTagToSring(status.MPI_TAG), status.MPI_SOURCE); break;
		default: LOG("\n[ID %d][recv] %s from %d ", ID, messageTagToSring(status.MPI_TAG), status.MPI_SOURCE);
	}*/
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
	int size[3];
	MPI_Aint shift[3];
	MPI_Aint address[4];
	MPI_Datatype types[3];

	if(!firstBuild){

		types[0] = MPI_INT; 
		types[1] = MPI_INT; 
		types[2] = MPI_PASSWORD_STRUCT;

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
		types[1] = MPI_INT;
		types[2] = MPI_PASSWORD_STRUCT;

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
	sprintf(tag,"Request{rangeMin:%d, rangeMax:%d, password:%s }",req.rangeMin,req.rangeMax,passwordToString(req.p));

	return tag;
}

char * responseToString(Response res){
	static char tag[TAG_SIZE];

	memset(tag,0,TAG_SIZE);
	sprintf(tag,"Response{id:%d, ntries: %d, password:%s }",res.taskId,res.ntries,passwordToString(res.p));

	return tag;
}

char * passwordStatusToString(PasswordStatus ps){
	int i;
	static char tag[TAG_SIZE];

	memset(tag,0,TAG_SIZE);
	sprintf(tag,"PasswordStatus{id:%d, finished:%d, lastAssigned:%d, taskIds:TaskID[",ps.passwordId,ps.finished,ps.lastAssigned);
	for(i=0; i<=ps.lastAssigned; i++)
		sprintf(tag,"%s%d, ",tag,ps.taskIds[i]);
	sprintf(tag,"\b]}");

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