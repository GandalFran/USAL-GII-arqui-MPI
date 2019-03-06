#include "utils.h"

#define DEBUG_LINE fprintf(stderr, "\n[%d:%s:%s]\n",__LINE__,__FILE__,__FUNCTION__);

//Behaviours
void calculationBehaviour();
void masterCommunicationBehaviour();

//auxiliar behaviour functions
bool doCalculus(Password * p, int rangeMin, int rangeMax);
bool requestGestion(Request request, Response * response);
void responseGestion(PasswordStatus * passwordStatusList, Response * responseList, Response res);
void taskAssignation(TaskID * taskToAssign, int nTasksToAssign, Password * passwordList, PasswordStatus * passwordStatusList, Request * masterReq);

//string builders
void printCurrentSituation(PasswordStatus * passwordStatusList, Password * passwordList);

//mpi facilities
int getId();
int getNtasks();
char * getProcessorName();
bool areThereAnyMsg();
void mySend(TaskID destinationAddr, int nElements, void * data, MPI_Datatype dataType, MessageTag tag);
MessageTag myRecv(TaskID destinationAddr, int nElements, void * data, MPI_Datatype dataType, MessageTag tag);
MPI_Datatype getMPI_PASSWORD_STRUCT();
MPI_Datatype getMPI_REQUEST_STRUCT();
MPI_Datatype getMPI_RESPONSE_STRUCT();


int main (int argc, char * argv[]){
	int seed, i;
	char identity[TAG_SIZE];

	//initialize the MPI
	MPI_Init(&argc, &argv);
	MPI_Errhandler_set(MPI_COMM_WORLD, MPI_ERRORS_RETURN); 

	//give the random seed 
	srand( seed = (ID + time(NULL)) );

	//save the initial process data
	sprintf(identity,"[ID %d][PID %d][PC: %s][SEED: %d]",ID,getpid(),getProcessorName(),seed);
	if(IS_MASTER(ID)){
		printf("\nTasks: %d Passwords: %d\n",N_TASKS,N_PASSWORDS);
		printf("\n%s",identity);
		for(i=0; i< N_TASKS-1; i++){
			myRecv(MPI_ANY_SOURCE, TAG_SIZE, identity, MPI_CHAR, IDENTITY);
			printf("\n%s",identity);
		}
		printf("\n");
	}else{
		mySend(MASTER_ID,TAG_SIZE,identity,MPI_CHAR,IDENTITY);
	}

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

void calculationBehaviour(){
	bool solvedByMe;
	MessageTag tag;
	Request request;
	Response response;

	do{
		//Reset the request and response, and wait to password request
		memset(&request,0,sizeof(Request));
		myRecv(MASTER_ID, 1, &request, MPI_REQUEST_STRUCT(request), DECODE_REQUEST);
		//Hangle re request
		solvedByMe = requestGestion(request,&response);
		if(solvedByMe)
			mySend(MASTER_ID,1,&response,MPI_RESPONSE_STRUCT(response),DECODE_RESPONSE);
		//recv the decode_stop or finalize to discard it
		tag = myRecv(MASTER_ID, 0, NULL, NULL_DATATYPE, MPI_ANY_TAG);
		if(FINALIZE == tag){
			EXIT(EXIT_SUCCESS);
			return;
		}
	}while(TRUE);
}

void masterCommunicationBehaviour(){
	bool solvedByMe;
	Response response;
	int nTasksToAssign;
	Request reqToMaster;
	TaskID * tasksToAssgin;
	int solvedPasswords, i;

	TaskID firstAssignation[MAX_PASSWORDS];

	Password passwordList[MAX_PASSWORDS];
	PasswordStatus passwordStatusList[MAX_PASSWORDS];
	Response responseList[MAX_PASSWORDS];

	//reset the control arrays and other vars
	memset(passwordList,0,MAX_PASSWORDS*sizeof(Password));
	memset(passwordStatusList,0,MAX_PASSWORDS*sizeof(PasswordStatus));
	memset(responseList,0,MAX_PASSWORDS*sizeof(Response));

	for(i=0; i<MAX_PASSWORDS; i++){
		firstAssignation[i] = i;
		passwordStatusList[i].passwordId = i;
		passwordStatusList[i].finished = FALSE;
	}

	nTasksToAssign = N_TASKS;
	tasksToAssgin = firstAssignation;

	//generate passwords
	for(i=0; i<N_PASSWORDS; i++){
		passwordList[i].passwordId = i;

		GET_RANDMON_SALT(passwordList[i].s);
		GET_RANDOM_STR_IN_BOUNDS(passwordList[i].decrypted,0,MAX_RAND);
		ENCRYPT(passwordList[i].decrypted, passwordList[i].encrypted, passwordList[i].s);
	}

	//wait for responses, and reasignate tasks to each free 
	for(solvedPasswords = 0; solvedPasswords < N_PASSWORDS; solvedPasswords++){
		//division of tasks: if is the firstTime, the asignation is to all tasks, if not, is to the tasks implicated in the last solved password
		taskAssignation(tasksToAssgin, nTasksToAssign, passwordList, passwordStatusList, &reqToMaster);

		//calculate the master assignement
		solvedByMe = requestGestion(reqToMaster,&response);

		//if the one who has solved isn't the master, wait for a response
		if(!solvedByMe)
			myRecv(MPI_ANY_SOURCE, 1, &response, MPI_RESPONSE_STRUCT(response), DECODE_RESPONSE);

		//stop all tasks working on the password except if it is the last time
		for(i=0; i<passwordStatusList[response.p.passwordId].numTasksDecrypting; i++){
			if(!IS_MASTER(passwordStatusList[response.p.passwordId].taskIds[i])){
				mySend(passwordStatusList[response.p.passwordId].taskIds[i], 0 , NULL, NULL_DATATYPE, (solvedPasswords!=N_PASSWORDS-1) ? DECODE_STOP : FINALIZE);
			}
		}

		//make the response treatment
		responseGestion(passwordStatusList,responseList,response);

		//save the last solved password id to assign tasks in the next iteration
		tasksToAssgin = passwordStatusList[response.p.passwordId].taskIds;
		nTasksToAssign = passwordStatusList[response.p.passwordId].numTasksDecrypting;
	}

	printCurrentSituation(passwordStatusList, passwordList);
}

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

bool requestGestion(Request request, Response * response){
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

		//Check if a message has been recived
		if( 0 == (response->ntries % NUMCHECKSMAIL)){
			if(areThereAnyMsg())
				return FALSE;
		}
		
	}while(TRUE);

}

void responseGestion(PasswordStatus * passwordStatusList, Response * responseList, Response res){
	PasswordID passwordId = res.p.passwordId;

	//mark password as solved
	passwordStatusList[passwordId].finished = TRUE;
	passwordStatusList[passwordId].solver = res.taskId;
	//mark the task as solved and save the response
	memcpy(&(responseList[passwordId]),&res,sizeof(Response));
}

void taskAssignation(TaskID * taskToAssign, int nTasksToAssign, Password * passwordList, PasswordStatus * passwordStatusList, Request * masterReq){
	Request req;
	int i, j, rangeIncrement;

	TaskID currentTask;
	bool isRequestSend [MAX_TASKS];
	bool hasBeenStopped[MAX_PASSWORDS];
	PasswordID passwordAssignedToTask[MAX_TASKS];
	PasswordID currentPassword, selectedPassword;

	memset(isRequestSend,0,sizeof(bool)*MAX_TASKS);
	memset(hasBeenStopped,0,sizeof(bool)*MAX_TASKS);

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

		//assign the password to the task
		passwordAssignedToTask[currentTask] = selectedPassword;

		//stop the current porcesses working in the password
		if(!hasBeenStopped[selectedPassword]){
			hasBeenStopped[selectedPassword] = TRUE;
			for(i=0; i<passwordStatusList[selectedPassword].numTasksDecrypting; i++){
				if(!IS_MASTER(passwordStatusList[selectedPassword].taskIds[i]))
					mySend(passwordStatusList[selectedPassword].taskIds[i], 0, NULL, NULL_DATATYPE, DECODE_STOP);
			}
		}

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
					mySend( passwordStatusList[currentPassword].taskIds[i], 1, &req, MPI_REQUEST_STRUCT(req), DECODE_REQUEST);

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


// ----------------------------------------------------------------------------------------------------------------
void printCurrentSituation(PasswordStatus * passwordStatusList, Password * passwordList){
	int i,j;

	char separator[]="+--+--------+-------------+------+-----------------------------------------------";
	char header[]=   "|ID|  NORMAL|    ENCRYPTED|SOLVER| TASKS WORKING";

	printf("\n%s\n%s",separator,header);

	for(i=0; i< N_PASSWORDS; i++){
		printf("\n%s",separator);
		
		printf("\n|%02d|%8s|%13s|",i,passwordList[i].decrypted,passwordList[i].encrypted);

		if(!passwordStatusList[i].finished)
			printf("  --  |");
		else
			printf("  %02d  |",passwordStatusList[i].solver);

		if(passwordStatusList[i].finished)
			printf(" SOLVED");
		else{
			if(passwordStatusList[i].numTasksDecrypting == 0)
				printf(" --");
			else{
				printf(" ");
				for(j=0; j<passwordStatusList[i].numTasksDecrypting; j++){
						printf("%02d ",passwordStatusList[i].taskIds[j]);
				}
			}
		}
	}

	printf("\n%s\n",separator);
}

//mpi utils
int getId(){
	static int internalId = -1;
	if(-1 == internalId) MPI_Comm_rank(MPI_COMM_WORLD, &internalId);
	return internalId;
}

int getNtasks(){
	static int ntasks = -1;
	if(-1 == ntasks) MPI_Comm_size(MPI_COMM_WORLD, &ntasks);
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

void mySend(TaskID destinationAddr, int nElements, void * data, MPI_Datatype dataType, MessageTag tag){
	MPI_Request req;
	EXIT_ON_FAILURE(MPI_Isend(data, nElements , dataType, destinationAddr, tag, MPI_COMM_WORLD, &req));
}

MessageTag myRecv(TaskID destinationAddr, int nElements, void * data, MPI_Datatype dataType, MessageTag tag){
	MPI_Status status;
	EXIT_ON_FAILURE(MPI_Recv(data, nElements, dataType, destinationAddr, tag, MPI_COMM_WORLD, &status));
	return status.MPI_TAG;
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
