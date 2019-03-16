#include "utils.h"

#define DEBUG_LINE fprintf(stderr, "\n[%d:%s:%s]\n",__LINE__,__FILE__,__FUNCTION__);

//Behaviours
void calculationBehaviour();
void communicationBehaviour();

//auxiliar behaviour functions
bool doCalculus(Password * p, int rangeMin, int rangeMax);
bool requestHandler(Request request, Response * response);
void responseHandler(PasswordStatus * passwordStatusList, Response res);
void taskDispatcher(TaskID * taskToAssign, int nTasksToAssign, Password * passwordList, PasswordStatus * passwordStatusList, Request * masterReq);

//IO
void printTaskStatus(PasswordStatus * passwordStatusList, Password * passwordList);
void exportToCsv(Password * passwordList, PasswordStatus * passwordStatusList, unsigned long long * triesPerTask, double totalTime);

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
	int i;
	void (*behaviour)(void);
	char identity[TAG_SIZE];

	//initialize the MPI
	MPI_Init(&argc, &argv);
	MPI_Errhandler_set(MPI_COMM_WORLD, MPI_ERRORS_RETURN); 

	//save the initial process data
	sprintf(identity,"[ID %d][PID %d][PC: %s][SEED: %d]",ID,getpid(),getProcessorName(),GET_SEED(ID));
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

	//set the behaviour
	behaviour = (IS_MASTER(ID)) ? communicationBehaviour : calculationBehaviour;

	//wait to all and then go to the behaviour
	MPI_Barrier(MPI_COMM_WORLD);
	behaviour();

	//finalize
	EXIT(EXIT_SUCCESS);
}

void calculationBehaviour(){
	bool solvedByMe;
	MessageTag tag;
	Request request;
	Response response;
	unsigned long long totalTries = 0;

	//give the seed
	srand(GET_SEED(ID));

	do{
		//Reset the request and response, and wait to password request
		memset(&request,0,sizeof(Request));
		myRecv(MASTER_ID, 1, &request, MPI_REQUEST_STRUCT(request), DECODE_REQUEST);

		//Hangle re request
		solvedByMe = requestHandler(request,&response);
		if(solvedByMe)
			mySend(MASTER_ID,1,&response,MPI_RESPONSE_STRUCT(response),DECODE_RESPONSE);
		//add the number of tries
		totalTries += response.ntries;

		//recv the decode_stop or finalize to discard it
		tag = myRecv(MASTER_ID, 0, NULL, NULL_DATATYPE, MPI_ANY_TAG);
		if(FINALIZE == tag){
			//send the total number of tries and finalize
			mySend(MASTER_ID, 1, &totalTries, MPI_UNSIGNED_LONG_LONG, FINALIZE_RESPONSE);
			return;
		}
	}while(TRUE);
}

void communicationBehaviour(){
	bool solvedByMe;
	int solvedPasswords, i;
	double start, end, totalTime;
	unsigned long long triesPerTask[MAX_TASKS], tmpTries = 0;

	Response response;
	Request reqToMaster;

	int nTasksToAssign;
	TaskID * tasksToAssgin;
	TaskID firstAssignation[MAX_PASSWORDS];

	Password passwordList[MAX_PASSWORDS];
	PasswordStatus passwordStatusList[MAX_PASSWORDS];

	//reset the control arrays and other vars
	memset(passwordList,0,MAX_PASSWORDS*sizeof(Password));
	memset(passwordStatusList,0,MAX_PASSWORDS*sizeof(PasswordStatus));
	memset(triesPerTask,0,MAX_PASSWORDS*sizeof(unsigned long long));

	for(i=0; i<MAX_PASSWORDS; i++)
		firstAssignation[i] = passwordStatusList[i].passwordId = i;

	nTasksToAssign = N_TASKS;
	tasksToAssgin = firstAssignation;

	//generate passwords
	for(i=0; i<N_PASSWORDS; i++){
		passwordList[i].id = i;
		GET_RANDMON_SALT(passwordList[i].s);
		GET_RANDOM_STR_IN_BOUNDS(passwordList[i].decrypted,0,MAX_RAND);
		ENCRYPT(passwordList[i].decrypted, passwordList[i].encrypted, passwordList[i].s);
	}

	//give the seed
	//NOTE: the seed is given after the password generation, to ensure that the process 0, 
	//		have a different seed while the password decrypting
	srand(GET_SEED(ID));

	start = MPI_Wtime();

	//wait for responses, and reasignate tasks to each free 
	for(solvedPasswords = 0; solvedPasswords < N_PASSWORDS; solvedPasswords++){
		//division of tasks: if is the firstTime, the asignation is to all tasks, if not, is to the tasks implicated in the last solved password
		taskDispatcher(tasksToAssgin, nTasksToAssign, passwordList, passwordStatusList, &reqToMaster);

		//calculate the master assignement and if the one who has solved isn't the master, wait for a response
		solvedByMe = requestHandler(reqToMaster,&response);
		if(!solvedByMe)
			myRecv(MPI_ANY_SOURCE, 1, &response, MPI_RESPONSE_STRUCT(response), DECODE_RESPONSE);

		//save here the end time, to have more accuracy on the total time
		end = MPI_Wtime();
		response.time = end - start;
		passwordStatusList[response.passwordId].usedTime = end - passwordStatusList[response.passwordId].usedTime;
		//save the response tries to the total tries
		tmpTries += response.ntries;

		//stop all tasks working on the password except if it is the last time
		for(i=0; i<passwordStatusList[response.passwordId].numTasksDecrypting; i++){
			if(!IS_MASTER(passwordStatusList[response.passwordId].taskIds[i])){
				mySend(passwordStatusList[response.passwordId].taskIds[i], 0 , NULL, NULL_DATATYPE, (solvedPasswords!=N_PASSWORDS-1) ? DECODE_STOP : FINALIZE);
			}
		}

		//make the response treatment
		responseHandler(passwordStatusList,response);

		//save the last solved password id to assign tasks in the next iteration
		tasksToAssgin = passwordStatusList[response.passwordId].taskIds;
		nTasksToAssign = passwordStatusList[response.passwordId].numTasksDecrypting;
	}

	//print the current situation one last time, and the time
	printTaskStatus(passwordStatusList, passwordList);

	//wait to the calculation tasks send us data
	totalTime = end - start;
	triesPerTask[0] = tmpTries;
	for(i=1; i<N_TASKS; i++){
		myRecv(i, 1, &triesPerTask[i], MPI_UNSIGNED_LONG_LONG, FINALIZE_RESPONSE);
	}

	exportToCsv(passwordList, passwordStatusList, triesPerTask, totalTime);
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

bool requestHandler(Request request, Response * response){
	long counter = 0;

	//Loop until password solved or a new response recived
	do{
		//increment the try number
		counter++;

		//do the calculus and then check if the solution has been found
		if(doCalculus(&(request.p),request.rangeMin,request.rangeMax)){
			memset(response,0,sizeof(Response));
			//fill the response
			response->taskId = ID;
			response->ntries = counter;
			response->passwordId = request.p.id;
			//return TRUE, to know before, that the master has finished the task
			return TRUE;
		}

		//Check if a message has been recived
		if( 0 == (counter % NUMCHECKSMAIL)){
			if(areThereAnyMsg()){
				response->ntries = counter;
				return FALSE;
			}
		}
		
	}while(TRUE);

}

void responseHandler(PasswordStatus * passwordStatusList, Response res){
	PasswordID passwordId = res.passwordId;
	passwordStatusList[passwordId].finished = TRUE;
	memcpy(&(passwordStatusList[passwordId].solverResponse),&res,sizeof(Response));
}

void taskDispatcher(TaskID * taskToAssign, int nTasksToAssign, Password * passwordList, PasswordStatus * passwordStatusList, Request * masterReq){
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
		if(0 == passwordStatusList[selectedPassword].numTasksDecrypting){
			passwordStatusList[selectedPassword].usedTime = MPI_Wtime();
		}
		passwordStatusList[selectedPassword].taskIds[passwordStatusList[selectedPassword].numTasksDecrypting] = taskToAssign[currentTask];
		passwordStatusList[selectedPassword].numTasksDecrypting++;
	}

	//foreach task, send the messages, and if two tasks are designed to the same password, do at the same time
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

				//send the request
				if( IS_MASTER(passwordStatusList[currentPassword].taskIds[i]) )
					memcpy(masterReq,&req,sizeof(Request));
				else
					mySend( passwordStatusList[currentPassword].taskIds[i], 1, &req, MPI_REQUEST_STRUCT(req), DECODE_REQUEST);
			}

			//mark in isRequestSend, which requests has been send
			for(i=0; i < nTasksToAssign; i++){
				if(passwordAssignedToTask[i] == currentPassword){
					isRequestSend[i] = TRUE;
				}
			}
		}
	}

	printTaskStatus(passwordStatusList, passwordList);
}


// ----------------------------------------------------------------------------------------------------------------
void printTaskStatus(PasswordStatus * passwordStatusList, Password * passwordList){
	int i,j;

	char separator[]="+----+--------+-------------+--------+--------+----------+---------------+---------------------------------";
	char header[]=   "| ID | NORMAL |  ENCRYPTED  | SOLVER |  TIME  |TIME TAKEN|NUMBER OF TRIES|  TASKS WORKING                  ";

	printf("\n%s\n%s",separator,header);

	for(i=0; i< N_PASSWORDS; i++){
		printf("\n%s",separator);
		
		//ID, decrypted and encrypted
		printf("\n| %02d |%8s|%13s|",i,passwordList[i].decrypted,passwordList[i].encrypted);

		//solver
		if(!passwordStatusList[i].finished)
			printf("   --   |");
		else
			printf("   %02d   |",passwordStatusList[i].solverResponse.taskId);

		//time
		if(!passwordStatusList[i].finished)
			printf("   --   |");
		else
			printf(" %6.2f |",passwordStatusList[i].solverResponse.time);

		//time taken
		if(!passwordStatusList[i].finished)
			printf("    --    |");
		else
			printf(" %8.2f |",passwordStatusList[i].usedTime);

		//number of tries
		if(!passwordStatusList[i].finished)
			printf("       --      |");
		else
			printf(" %13llu |",passwordStatusList[i].solverResponse.ntries);

		//working status
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


void exportToCsv(Password * passwordList, PasswordStatus * passwordStatusList, unsigned long long * triesPerTask, double totalTime){
	int i;
	FILE * f;
	unsigned long long totalTries;

	f = fopen(OUTPUT_FILE,"w+");

	//export the passwords 
	fprintf(f,"\nPasswords:");
	for(i=0; i< N_PASSWORDS; i++){
		fprintf(f,"\n%d %s %s %d %.2f %.2f %lld",
			passwordList[i].id,
			passwordList[i].decrypted,
			passwordList[i].encrypted,
			passwordStatusList[i].solverResponse.taskId,
			passwordStatusList[i].solverResponse.time,
			passwordStatusList[i].usedTime,
			passwordStatusList[i].solverResponse.ntries
		);
	}

	//export the tries per task
	fprintf(f,"\n\nTries per task:");
	for(i=0; i< N_TASKS; i++){
		totalTries += triesPerTask[i];
		fprintf(f,"\n\t%d %llu",i,triesPerTask[i]);
	}

	//export the rest of the data
	fprintf(f,"\n\nTIME\t%.2f\nTRIES\t%lld\nNUM PROCS:\t%d\n",totalTime,totalTries,N_TASKS);
	printf("\n\nTIME\t%.2f\nTRIES\t%lld\nNUM PROCS:\t%d\n",totalTime,totalTries,N_TASKS);

	fclose(f);
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

//Wrap the send, recv, ... and more communication tasks
bool areThereAnyMsg(){
	int result;
	MPI_Status status;
	MPI_Iprobe( MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &result, &status);
	return (result != 0) ? TRUE : FALSE;
}

void mySend(TaskID destinationAddr, int nElements, void * data, MPI_Datatype dataType, MessageTag tag){
	MPI_Request req;
	MPI_Isend(data, nElements , dataType, destinationAddr, tag, MPI_COMM_WORLD, &req);
}

MessageTag myRecv(TaskID destinationAddr, int nElements, void * data, MPI_Datatype dataType, MessageTag tag){
	MPI_Status status;
	MPI_Recv(data, nElements, dataType, destinationAddr, tag, MPI_COMM_WORLD, &status);
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
	MPI_Address(&(p.id),&address[2]);
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

	types[0] = MPI_UNSIGNED_LONG_LONG; 
	types[1] = MPI_DOUBLE; 
	types[2] = MPI_INT;
	types[3] = MPI_INT;

	size[0] = 1;
	size[1] = 1;
	size[2] = 1;
	size[3] = 1;

	MPI_Address(&res,&address[0]);
	MPI_Address(&(res.ntries),&address[1]);
	MPI_Address(&(res.time),&address[2]);
	MPI_Address(&(res.taskId),&address[3]);
	MPI_Address(&(res.passwordId),&address[4]);
		
	shift[0] = address[1] - address[0];
	shift[1] = address[2] - address[0];
	shift[2] = address[3] - address[0];
	shift[3] = address[4] - address[0];

	MPI_Type_struct(4,size,shift,types,&dataType);
	MPI_Type_commit(&dataType);

	return dataType;
}
