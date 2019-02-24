
#include "header.h"

//Task definition
void communicationTask();
void calculationTask();
void masterCalculationTask();

//Calculus definition
bool doCalculus(Password * p, SaltPointer salt);

//task division
void doTaskDivision(Request * requestList, Work * taskSituation);

//Wrapping and treatment of MPI library
int getId();
int getNtasks();

//auxiliar
char * passwordToString(Password p);
char * requestToString(Request req);
char * responseToString(Response res);


// -------------------------------- Main --------------------------------

int main (int argc, char * argv[]){

	//initialize the MPI
	MPI_Init(&argc, &argv);
	LOG("\n[ID:%d] Started",ID);

	//Taking each one to his role
	if(IS_MASTER(ID)){
		communicationTask();
	}else{
		//here it is not neccesary to send a p
		calculationTask();
	}

	//Finalize execution
	MPI_EXIT(EXIT_SUCCESS);
}


// -------------------------------- Task definition --------------------------------
void communicationTask(){
	Salt salt;
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

		GET_RANDMON_SALT(salt);
		GET_RANDOM_STR_IN_BOUNDS(requestList[i].p.decrypted,0,MAX_RAND);
		strcpy(requestList[i].p.encrypted, crypt(requestList[i].p.decrypted,salt));
		
		LOG("\n[ID %d][Generated] %s",ID,passwordToString(requestList[i].p));
	}


	for(solvedTasks = 0; solvedTasks < NTASKS; solvedTasks++){
		//Division of tasks
		doTaskDivision(requestList,taskSituation);

		//go to the calculation -> there a REQUEST for calculus will be recived
		masterCalculationTask();

		//recv	

		//-----response gestion------
		LOG("\n[ID %d][Recived response] %s",ID,responseToString(resTmp));

		//mark password as solved
		requestList[resTmp.p.passwordId].finished = TRUE;

		//advise all tasks that are solving this password, to stop
		for(i =0; i< NTASKS; i++){
			if( taskSituation[i].passwordId == resTmp.p.passwordId ){
				//send a stop message
			}
		}

		//if neccesary save here the response, to do statistics before

	}

}

void calculationTask(){
	Salt salt;
	Request request;
	Response response;
	bool recivedFinalizeOrder = FALSE;

	//reset the response
	memset(&response,0,sizeof(Response));
	response.taskId = ID;

	do{

		//Reset the request and wait to password request or finalize
		memset(&request,0,sizeof(Request));
		//recv
		LOG("\n[ID %d][Recived] %s",ID,requestToString(request));

		//obtain the salt
		GET_SALT(salt,request.p.encrypted);

		do{
			//increment the try number
			response.ntries++;

			//do the calculus and then check if the solution has been found
			if(doCalculus(&(request.p),salt)){
				//fill the response
				memcpy(&(response.p),&(request.p),sizeof(Password));
				LOG("\n[ID %d][Solution found] %s",ID,responseToString(response));
				
				//send

				//go to the external loop
				break;
			}

			//Non-blocking check if Master has ordered to stop, or to finalize our calculation

		}while(TRUE);
	
	}while(!recivedFinalizeOrder);
}

void masterCalculationTask(){
	Salt salt;
	Request request;
	Response response;

	//reset the response
	memset(&response,0,sizeof(Response));
	response.taskId = ID;

	//Reset the request and wait to password request or finalize
	memset(&request,0,sizeof(Request));
	//recv
	LOG("\n[ID %d][Recived] %s",ID,requestToString(request));

	//obtain the salt
	GET_SALT(salt,request.p.encrypted);

	do{
		//increment the try number
		response.ntries++;

		//do the calculus and then check if the solution has been found
		if(doCalculus(&(request.p),salt)){
			//fill the response
			memcpy(&(response.p),&(request.p),sizeof(Password));
			LOG("\n[ID %d][Solution found] %s",ID,responseToString(response));

			//send

			//finalize the decoding 
			break;
		}

		//Check if a message has been recived --> its neccesary to handle a request an send

	}while(TRUE);
}

// -------------------------------- Calculus definition --------------------------------

bool doCalculus(Password * p, SaltPointer salt){
	char possibleSolution[PASSWORD_SIZE], *possibleSolutionEncripted = NULL;

	//generate and encript possible solution
	GET_RANDOM_STR_IN_BOUNDS(possibleSolution,0,MAX_RAND);
	possibleSolutionEncripted = crypt(possibleSolution,salt);

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

	//for the moment, all the tasks are send to the 0 task 

	for(i=0; i<NTASKS; i++){
		if(!requestList[i].finished){
			//send message to main task 
			return;
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


//Wrap the send and recive, to do a better logging
void send(TaskID destinationAddr, void * data, int nElements, MPI_Datatype tipo_datos, MessageTag tag){
	LOG("\n[ID %d][Send] Destination: %d, type:%s", ID, destinationAddr, MESSAGE_TAG_TOSTRING(tag));
	MPI_Send(data, nElements, MPI_Datatype tipo_datos, destinationAddr, tag, MPI_COMM_WORLD);
}

void recv(TaskID destinationAddr, void * data, int nElements, MPI_Datatype tipo_datos, MessageTag tag){
	MPI_Send(data, nElements, MPI_Datatype tipo_datos, destinationAddr, tag, MPI_COMM_WORLD);
	LOG("\n[ID %d][recv] from: %d, type:%s", ID, destinationAddr, MESSAGE_TAG_TOSTRING(tag));
}


// -------------------------------- Auxiliar --------------------------------

char * passwordToString(Password p){
	static char tag[TAG_SIZE];

	memset(tag,0,TAG_SIZE);
	sprintf(tag,"Password{id:%d, decrypted:%s, encrypted: %s, salt: %c%c}",p.passwordId,p.decrypted,p.encrypted,p.encrypted[0],p.encrypted[1]);

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