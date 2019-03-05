
#include <stdlib.h>
#include <crypt.h>
#include <time.h>
#include <unistd.h>
#include "toString.h"
#include "dataDefinition.h"
#include "mpiUtils.h"
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


// -------------------------------- Main --------------------------------

int main (int argc, char * argv[]){
	int seed;

	//initialize the MPI
	MPI_Init(&argc, &argv);
	MPI_Errhandler_set(MPI_COMM_WORLD, MPI_ERRORS_RETURN); 

	//give the random seed 
	srand( seed = (ID + time(NULL)) );

	//log the initial process data
	LOG("\n[ID %d][PID %d][PC: %s][SEED: %d] Started",ID,getpid(),"",seed);

	//wait to all
	MPI_Barrier(MPI_COMM_WORLD);

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

	//include the data of each process-> ID, computer, seed


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
		if(lastSolvedPasswordId < 0)
			taskAssignation(firstAssignation, nTasksToAssign, passwordList, passwordStatusList, &reqToMaster);
		else
			taskAssignation(passwordStatusList[lastSolvedPasswordId].taskIds, nTasksToAssign, passwordList, passwordStatusList, &reqToMaster);

		//calculate the master assignement
		solvedByMe = masterCalculationBehaviour(reqToMaster,&resTmp);

		//if the one who has solved isn't the master, wait for a response
		if(!solvedByMe)
			recv(MPI_ANY_SOURCE, &resTmp, MPI_RESPONSE_STRUCT(resTmp), DECODE_RESPONSE);

		//make the response treatment
		responseGestion(passwordStatusList,responseList,resTmp);

		//save the last solved password id to assign tasks in the next iteration
		lastSolvedPasswordId = resTmp.p.passwordId;
		nTasksToAssign = passwordStatusList[lastSolvedPasswordId].numTasksDecrypting;
	}

	//finalize all tasks
	for(i=1; i< N_TASKS; i++){
		send(i, NULL , MPI_DATATYPE_NULL , FINALIZE);
	}

}

void responseGestion(PasswordStatus * passwordStatusList, Response * responseList, Response res){
	int i;
	PasswordID passwordId = res.p.passwordId;

	//mark password as solved
	passwordStatusList[passwordId].finished = TRUE;

	//mark the task as solved and save the response
	passwordStatusList[passwordId].finished = TRUE;
	memcpy(&(responseList[passwordId]),&res,sizeof(Response));

	LOG("\n[ID %d] ID %d <- %s %s", ID, res.taskId, passwordStatusToString(passwordStatusList[passwordId]), responseToString(res));
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
					send( passwordStatusList[currentPassword].taskIds[i], &req, MPI_REQUEST_STRUCT(req), DECODE_REQUEST);

				LOG("\n[ID %d] ID %d -> %s", ID, passwordStatusList[currentPassword].taskIds[i], requestToString(req));
			}

			//mark in isRequestSend, which requests has been send
			for(i=0; i < nTasksToAssign; i++){
				if(passwordAssignedToTask[i] == currentPassword){
					isRequestSend[i] = TRUE;
				}
			}
		}
	}
}


bool masterCalculationBehaviour(Request request, Response * response){
	double start;

	//reset the response
	memset(response,0,sizeof(Response));
	response->taskId = ID;

	start = MPI_Wtime();

	//Loop until password solved or a new response recived
	do{
		//increment the try number
		(response->ntries)++;

		//do the calculus and then check if the solution has been found
		if(doCalculus(&(request.p),request.rangeMin,request.rangeMax)){
			//fill the response
			response->time = MPI_Wtime() - start;
			memcpy(&(response->p),&(request.p),sizeof(Password));
			//return TRUE, to know before, that the master has finished the task
			return TRUE;
		}

		//Check if a message has been recived --> its neccesary to handle a request an send
		if(areThereAnyMsg())
			return FALSE;
		
	}while(TRUE);

}

// -------------------------------- Calculation task behaviour --------------------------------

void calculationBehaviour(){
	Request request;
	Response response;
	double start;

	do{
		//Reset the request and response, and wait to password request
		memset(&response,0,sizeof(Response));
		memset(&request,0,sizeof(Request));
		recv(MASTER_ID, &request, MPI_REQUEST_STRUCT(request), MPI_ANY_TAG);

		start = MPI_Wtime();

		//Loop until password solved or a new order recived
		do{
			//increment the try number
			response.ntries++;

			//do the calculus and then check if the solution has been found
			if(doCalculus(&(request.p),request.rangeMin,request.rangeMax)){
				//fill the response and send to master
				response.taskId = ID;
				response.time = MPI_Wtime() - start;
				memcpy(&(response.p),&(request.p),sizeof(Password));
				send(MASTER_ID, &response, MPI_RESPONSE_STRUCT(response), DECODE_RESPONSE);
				//go to the external loop
				break;
			}

			//Non-blocking check if Master has send another order -> range change or password solved
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
