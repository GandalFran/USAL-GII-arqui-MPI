#include "utils.h"

#define DEBUG_LINE fprintf(stderr, "\n[%d:%s:%s]\n",__LINE__,__FILE__,__FUNCTION__);

//Communication master behaviour
void masterCommunicationBehaviour();

void taskAssignation(TaskID * taskToAssign, int nTasksToAssign, Password * passwordList, PasswordStatus * passwordStatusList, Request * masterReq);
bool masterCalculationBehaviour(Request request, Response * response);
void responseGestion(PasswordStatus * passwordStatusList, Response * responseList, Response res);

//Calculation task behaviour
void calculationBehaviour();

//Calculus definition
bool doCalculus(Password * p, int rangeMin, int rangeMax);

//string builders
void printCurrentSituation(PasswordStatus * passwordStatusList, Password * passwordList);

char * passwordToString(Password p);
char * requestToString(Request req);
char * responseToString(Response res);
char * passwordStatusToString(PasswordStatus ps);
char * messageTagToSring(MessageTag msg);

//mpi facilities
int getId();
int getNtasks();
char * getProcessorName();

bool areThereAnyMsg();
void mySend(TaskID destinationAddr, void * data, MPI_Datatype tipo_datos, MessageTag tag);
void myRecv(TaskID destinationAddr, void * data, MPI_Datatype tipo_datos, MessageTag tag);

MPI_Datatype getMPI_PASSWORD_STRUCT();
MPI_Datatype getMPI_REQUEST_STRUCT();
MPI_Datatype getMPI_RESPONSE_STRUCT();




// -------------------------------- Main --------------------------------

int main (int argc, char * argv[]){
	int seed;

	//initialize the MPI
	MPI_Init(&argc, &argv);
	MPI_Errhandler_set(MPI_COMM_WORLD, MPI_ERRORS_RETURN); 

	//give the random seed 
	srand( seed = (ID + time(NULL)) );

	//log the initial process data
	LOG("\n[ID %d][PID %d][PC: %s][SEED: %d] Started",ID,getpid(),getProcessorName(),seed);

	//wait to all
	MPI_Barrier(MPI_COMM_WORLD);

	//Taking each one to his role
	if(IS_MASTER(ID)){
		masterCommunicationBehaviour();
	}else{
		calculationBehaviour();
	}

	//Finalize execution
	EXIT(EXIT_SUCCESS);
}


// -------------------------------- Communication task behaviour --------------------------------
void masterCommunicationBehaviour(){
	bool solvedByMe;
	Response resTmp;
	Request reqToMaster;
	int nTasksToAssign;
	int solvedPasswords, i, lastSolvedPasswordId;

	TaskID firstAssignation[MAX_PASSWORDS];

	Password passwordList[MAX_PASSWORDS];
	PasswordStatus passwordStatusList[MAX_PASSWORDS];
	Response responseList[MAX_PASSWORDS];

	//reset the control arrays and other vars
	memset(passwordList,0,MAX_PASSWORDS*sizeof(Password));
	memset(passwordStatusList,0,MAX_PASSWORDS*sizeof(PasswordStatus));
	memset(responseList,0,MAX_PASSWORDS*sizeof(Response));

	for(i=0; i<MAX_PASSWORDS; i++){
		passwordStatusList[i].passwordId = i;
		passwordStatusList[i].numTasksDecrypting = 0;
		passwordStatusList[i].finished = FALSE;

		firstAssignation[i] = i;
	}

	nTasksToAssign = N_TASKS;
	lastSolvedPasswordId = -1;

	//generate passwords
	LOG("\n\n");
	for(i=0; i<N_PASSWORDS; i++){
		passwordList[i].passwordId = i;

		GET_RANDMON_SALT(passwordList[i].s);
		GET_RANDOM_STR_IN_BOUNDS(passwordList[i].decrypted,0,MAX_RAND);
		ENCRYPT(passwordList[i].decrypted, passwordList[i].encrypted, passwordList[i].s);
		
		LOG("\n\t%02d) %s -> %s",i,passwordList[i].decrypted,passwordList[i].encrypted);
	}

	//wait for responses, and reasignate tasks to each free 
	for(solvedPasswords = 0; solvedPasswords < N_PASSWORDS; solvedPasswords++){
		//division of tasks: if is the firstTime, the asignation is to all tasks, if not, is to the tasks implicated in the last solved password
		if(lastSolvedPasswordId < 0)
			taskAssignation(firstAssignation, nTasksToAssign, passwordList, passwordStatusList, &reqToMaster);
		else
			taskAssignation(passwordStatusList[lastSolvedPasswordId].taskIds, nTasksToAssign, passwordList, passwordStatusList, &reqToMaster);

		//calculate the master assignement
		solvedByMe = masterCalculationBehaviour(reqToMaster,&resTmp);

		//if the one who has solved isn't the master, wait for a response
		if(!solvedByMe)
			myRecv(MPI_ANY_SOURCE, &resTmp, MPI_RESPONSE_STRUCT(resTmp), DECODE_RESPONSE);

		//make the response treatment
		responseGestion(passwordStatusList,responseList,resTmp);

		//save the last solved password id to assign tasks in the next iteration
		lastSolvedPasswordId = resTmp.p.passwordId;
		nTasksToAssign = passwordStatusList[lastSolvedPasswordId].numTasksDecrypting;
	}

	printCurrentSituation(passwordStatusList, passwordList);

	//finalize all tasks
	for(i=1; i< N_TASKS; i++){
		mySend(i, NULL , MPI_DATATYPE_NULL , FINALIZE);
	}

}

void responseGestion(PasswordStatus * passwordStatusList, Response * responseList, Response res){
	int i;
	PasswordID passwordId = res.p.passwordId;

	//mark password as solved
	passwordStatusList[passwordId].finished = TRUE;
	passwordStatusList[passwordId].solver = res.taskId;

	//mark the task as solved and save the response
	memcpy(&(responseList[passwordId]),&res,sizeof(Response));

	//LOG("\n[ID %d] ID %d <- %s %s", ID, res.taskId, passwordStatusToString(passwordStatusList[passwordId]), responseToString(res));
}

void taskAssignation(TaskID * taskToAssign, int nTasksToAssign, Password * passwordList, PasswordStatus * passwordStatusList, Request * masterReq){
	Request req;
	int i, j, rangeIncrement;

	TaskID currentTask;
	bool isRequestSend [MAX_TASKS];
	PasswordID passwordAssignedToTask[MAX_TASKS];
	PasswordID currentPassword, selectedPassword;


	memset(isRequestSend,0,sizeof(bool)*MAX_TASKS);

	//foreach task, we search the optimun password to assign
	for(currentTask=0; currentTask < nTasksToAssign; currentTask++){
		//seach the PasswordID with the smalles number of tasks solving
		selectedPassword = 0;
		for(currentPassword=0; currentPassword<N_PASSWORDS; currentPassword++){
			if( (FALSE==passwordStatusList[currentPassword].finished && passwordStatusList[currentPassword].numTasksDecrypting < passwordStatusList[selectedPassword].numTasksDecrypting)
			|| TRUE == passwordStatusList[selectedPassword].finished ){
				selectedPassword = currentPassword;
			}
		}

		//assign the password to the task and
		passwordAssignedToTask[currentTask] = selectedPassword;

		//update the selected password status
		passwordStatusList[selectedPassword].taskIds[passwordStatusList[selectedPassword].numTasksDecrypting] = taskToAssign[currentTask];
		passwordStatusList[selectedPassword].numTasksDecrypting++;
	}

	//foreach task, mySend the messages, and if two tasks are designed to the same password, do at the same time
	for(currentTask=0; currentTask < nTasksToAssign; currentTask++){
		if(!isRequestSend[currentTask]){
			currentPassword = passwordAssignedToTask[currentTask];

			//calculate the range increments
			rangeIncrement = (int) (((int) MAX_RAND) / passwordStatusList[currentPassword].numTasksDecrypting);

			//fill the request structure
			req.rangeMin = req.rangeMax = -1;
			memcpy(&(req.p),&(passwordList[currentPassword]), sizeof(Password));
			memset(req.p.decrypted,0,PASSWORD_SIZE);

			//Send the new requests to each task assigned to currentPassword
			for(i=0; i<passwordStatusList[currentPassword].numTasksDecrypting; i++){

				//refresh the ranges -> NOTE:if is the last assignement, the range limit is the maximum 
				req.rangeMin = req.rangeMax + 1;
				req.rangeMax = ( i == passwordStatusList[currentPassword].numTasksDecrypting-1 ) ? (MAX_RAND) : (req.rangeMin + rangeIncrement);

				//mySend the request
				if( IS_MASTER(passwordStatusList[currentPassword].taskIds[i]) )
					memcpy(masterReq,&req,sizeof(Request));
				else
					mySend( passwordStatusList[currentPassword].taskIds[i], &req, MPI_REQUEST_STRUCT(req), DECODE_REQUEST);

				//LOG("\n[ID %d] ID %d -> %s", ID, passwordStatusList[currentPassword].taskIds[i], requestToString(req));
			}

			//mark in isRequestSend, which requests has been mySend
			for(i=0; i < nTasksToAssign; i++){
				if(passwordAssignedToTask[i] == currentPassword){
					isRequestSend[i] = TRUE;
				}
			}
		}
	}

	printCurrentSituation(passwordStatusList, passwordList);
}


bool masterCalculationBehaviour(Request request, Response * response){
	double start;

	//save the start time
	start = MPI_Wtime();

	//Loop until password solved or a new response recived
	do{
		//increment the try number
		(response->ntries)++;

		//do the calculus and then check if the solution has been found
		if(doCalculus(&(request.p),request.rangeMin,request.rangeMax)){
			//fill the response
			memset(response,0,sizeof(Response));
			response->taskId = ID;
			response->time = MPI_Wtime() - start;
			memcpy(&(response->p),&(request.p),sizeof(Password));
			//return TRUE, to know before, that the master has finished the task
			return TRUE;
		}

		//Check if a message has been recived --> its neccesary to handle a request an mySend
		if(areThereAnyMsg())
			return FALSE;
		
	}while(TRUE);

}

// -------------------------------- Calculation task behaviour --------------------------------

void calculationBehaviour(){
	double startTime;
	Request request;
	Response response;
	double start;

	do{
		//Reset the request and response, and wait to password request
		memset(&request,0,sizeof(Request));
		myRecv(MASTER_ID, &request, MPI_REQUEST_STRUCT(request), MPI_ANY_TAG);

		start = MPI_Wtime();

		//Loop until password solved or a new order recived
		do{
			//increment the try number
			response.ntries++;

			//do the calculus and then check if the solution has been found
			if(doCalculus(&(request.p),request.rangeMin,request.rangeMax)){
				//fill the response and mySend to master
				memset(&response,0,sizeof(Response));
				response.taskId = ID;
				response.time = MPI_Wtime() - start;
				memcpy(&(response.p),&(request.p),sizeof(Password));
				mySend(MASTER_ID, &response, MPI_RESPONSE_STRUCT(response), DECODE_RESPONSE);
				//go to the external loop
				break;
			}

			//Non-blocking check if Master has mySend another order -> range change or password solved
			if(areThereAnyMsg())
				break;
			
		}while(TRUE);
	
	}while(TRUE);
}

// -------------------------------- Calculus definition --------------------------------

bool doCalculus(Password * p, int rangeMin, int rangeMax){
	char possibleSolution[PASSWORD_SIZE], possibleSolutionEncripted[PASSWORD_SIZE];

	//generate and encrypt possible solution
	GET_RANDOM_STR_IN_BOUNDS(possibleSolution,rangeMin,rangeMax);
	ENCRYPT(possibleSolution, possibleSolutionEncripted, p->s);

	//check if is the possible solution is equal to the encripted data
	if ( IS_EQUAL_TO_STRING(possibleSolutionEncripted,p->encrypted) )
	{
		strcpy(p->decrypted,possibleSolution);
		return TRUE;
	}
	return FALSE;
}



void printCurrentSituation(PasswordStatus * passwordStatusList, Password * passwordList){
	int i,j;

	LOG("\n\n>-------------------------------------------------------------<\n");

	for(i=0; i<N_PASSWORDS; i++){
		LOG("\n\t%02d) %s -> ", i, passwordList[i].encrypted);
		if(!passwordStatusList[i].finished){
			LOG("    null -> ");
			if(passwordStatusList[i].numTasksDecrypting > 0){
				for(j=0; j<passwordStatusList[i].numTasksDecrypting; j++){
					LOG("%d ",passwordStatusList[i].taskIds[j]);
				}
			}else{
				LOG("none");
			}
		}else{
			LOG("%-8s -> %d",passwordList[i].decrypted,passwordStatusList[i].solver);
		}
	}
}

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
	sprintf(tag,"Response{id:%d, ntries: %d, password:%s time:%ld}",res.taskId,res.ntries,passwordToString(res.p),res.time);

	return tag;
}

char * passwordStatusToString(PasswordStatus ps){
	int i;
	static char tag[TAG_SIZE];

	memset(tag,0,TAG_SIZE);
	sprintf(tag,"PasswordStatus{id:%d, finished:%d, numTasksDecrypting:%d, taskIds:TaskID[",ps.passwordId,ps.finished,ps.numTasksDecrypting);
	for(i=0; i<ps.numTasksDecrypting; i++) sprintf(tag,"%s%d, ",tag,ps.taskIds[i]);
	sprintf(tag,"%s\b\b]}",tag);

	return tag;
}

char * messageTagToSring(MessageTag msg){
	static char tag[TAG_SIZE];

	memset(tag,0,TAG_SIZE);
	switch(msg){
		case DECODE_REQUEST: 	strcpy(tag,"DECODE_REQUEST");	break;
		case DECODE_RESPONSE: 	strcpy(tag,"DECODE_RESPONSE");	break;
		case FINALIZE: 			strcpy(tag,"FINALIZE");			break;
		default: sprintf(tag,"UNKNOWN:%d",msg);
	}

	return tag;
}

/////////////////////////////////////////////////////////////////////////////////////


int getId(){
	static int internalId = -1;

	if(-1 == internalId)
		MPI_Comm_rank(MPI_COMM_WORLD, &internalId);
	
	return internalId;
}

int getNtasks(){
	static int ntasks = -1;

	if(-1 == ntasks)
		MPI_Comm_size(MPI_COMM_WORLD, &ntasks);
	
	return ntasks;
}

char * getProcessorName(){
	static char name[TAG_SIZE];
	static bool solved = FALSE;

	if(!solved){
		solved = TRUE;
		int foo;
		MPI_Get_processor_name(name,&foo);
	}

	return name;
}

//Wrap the mySend, myRecv, ... and more communication tasks
bool areThereAnyMsg(){
	int result;
	MPI_Status status;
	EXIT_ON_FAILURE(MPI_Iprobe( MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &result, &status));
	return (result != 0) ? TRUE : FALSE;
}

void mySend(TaskID destinationAddr, void * data, MPI_Datatype tipo_datos, MessageTag tag){

	if(NULL != data)
		EXIT_ON_FAILURE(MPI_Send(data, 1, tipo_datos, destinationAddr, tag, MPI_COMM_WORLD));
	else{
		int foo = 1;
		EXIT_ON_FAILURE(MPI_Send(&foo, 1, MPI_INT, destinationAddr, tag, MPI_COMM_WORLD));
		//EXIT_ON_FAILURE(MPI_Send(NULL, 1, MPI_DATATYPE_NULL, destinationAddr, tag, MPI_COMM_WORLD));
	}

}

void myRecv(TaskID destinationAddr, void * data, MPI_Datatype tipo_datos, MessageTag tag){
	MPI_Status status;
	int foo;

	if(NULL != data)
		EXIT_ON_FAILURE(MPI_Recv(data, 1, tipo_datos, destinationAddr, tag, MPI_COMM_WORLD, &status));
	else{
		EXIT_ON_FAILURE(MPI_Recv(&foo, 1, MPI_INT, destinationAddr, tag, MPI_COMM_WORLD, &status));
		//EXIT_ON_FAILURE(MPI_Recv(NULL, 1, MPI_DATATYPE_NULL, destinationAddr, tag, MPI_COMM_WORLD, &status));
	}

	if(status.MPI_TAG == FINALIZE)
		EXIT(EXIT_SUCCESS);
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
	int size[4];
	MPI_Aint shift[4];
	MPI_Aint address[5];
	MPI_Datatype types[4];

	types[0] = MPI_INT; 
	types[1] = MPI_LONG; 
	types[2] = MPI_INT;
	types[3] = MPI_PASSWORD_STRUCT(res.p);

	size[0] = 1;
	size[1] = 1;
	size[2] = 1;
	size[3] = 1;

	MPI_Address(&res,&address[0]);
	MPI_Address(&(res.ntries),&address[1]);
	MPI_Address(&(res.time),&address[2]);
	MPI_Address(&(res.taskId),&address[3]);
	MPI_Address(&(res.p),&address[4]);
		
	shift[0] = address[1] - address[0];
	shift[1] = address[2] - address[0];
	shift[2] = address[3] - address[0];
	shift[3] = address[3] - address[0];

	MPI_Type_struct(4,size,shift,types,&dataType);
	MPI_Type_commit(&dataType);

	return dataType;
}
