
#include <stdlib.h>
#include <crypt.h>
#include <time.h>
#include <unistd.h>
#include "toString.h"
#include "dataDefinition.h"
#include "mpiUtils.h"
#include "utils.h"


//Communication master behaviour
void masterCommunicationBehaviour();

void taskAssignation(TaskID task,Password * passwordList, PasswordStatus * passwordStatusList, Request * masterReq);
bool masterCalculationBehaviour(Request request, Response * response);
void responseGestion(PasswordStatus * passwordStatusList, Response * responseList, Response res);

//Calculation task behaviour
void calculationBehaviour();

//Calculus definition
bool doCalculus(Password * p, int rangeMin, int rangeMax);


// -------------------------------- Main --------------------------------

int main (int argc, char * argv[]){

	//initialize the MPI
	MPI_Init(&argc, &argv);
	MPI_Errhandler_set(MPI_COMM_WORLD, MPI_ERRORS_RETURN); 
	LOG("\n[ID %d][PID %d] Started",ID,getpid());

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
		passwordStatusList[i].passwordId = i;
		passwordStatusList[i].lastAssigned = -1;
		passwordStatusList[i].finished = FALSE;
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

		//calculate the master assignement
		solvedByMe = masterCalculationBehaviour(reqToMaster,&resTmp);
		//wait for a response
		if(!solvedByMe)
			recv(MPI_ANY_SOURCE, &resTmp, MPI_RESPONSE_STRUCT(resTmp), DECODE_RESPONSE);

		//make the response treatment
		responseGestion(passwordStatusList,responseList,resTmp);
		//save the last solved password id to assign tasks in the next iteration
		lastSolvedPasswordId = resTmp.p.passwordId;
	}

	//finalize all tasks
	for(i=1; i< NTASKS; i++){
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

	LOG("\n[ID %d] ID %d <- %s", ID, res.taskId, passwordStatusToString(passwordStatusList[passwordId]));
}

void taskAssignation(TaskID task,Password * passwordList, PasswordStatus * passwordStatusList, Request * masterReq){
	Request req;
	int i, j, rangeIncrement;

	//check if a password is not assigned, and in that case, the password is assigned
	for(i=0; i< N_PASSWORDS; i++){
		//the initial value of lastAssigned is -1, because there are no processes decoding
		if(-1 == passwordStatusList[i].lastAssigned){
			//fill the request structure
			req.rangeMin = 0;
			req.rangeMax = MAX_RAND;
			memcpy(&(req.p),&(passwordList[i]), sizeof(Password));
			memset(req.p.decrypted,0,PASSWORD_SIZE);

			//save the new password status
			(passwordStatusList[i].lastAssigned)++;
			passwordStatusList[i].taskIds[passwordStatusList[i].lastAssigned] = task;

			//send the message to task if is not for master
			if( IS_MASTER(task) )
				memcpy(masterReq,&req,sizeof(Request));
			else
				send(task, &req, MPI_REQUEST_STRUCT(req), DECODE_REQUEST);

			LOG("\n[ID %d] ID %d -> %s", ID, task, requestToString(req));
			return;
		}
	}

	//if all tasks are assigned, then is assigned the first task not solved
	for(i=0; i< N_PASSWORDS; i++){
		if(FALSE == passwordStatusList[i].finished){

			//save the new password status
			(passwordStatusList[i].lastAssigned)++;
			passwordStatusList[i].taskIds[passwordStatusList[i].lastAssigned] = task;

			//calculate the range increments
			rangeIncrement = (int) MAX_RAND / (passwordStatusList[i].lastAssigned + 1);

			//fill the request structure
			req.rangeMin = -1;
			req.rangeMax = -1;
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
					send( passwordStatusList[i].taskIds[j], &req, MPI_REQUEST_STRUCT(req), DECODE_REQUEST);

				LOG("\n[ID %d] ID %d -> %s", ID, passwordStatusList[i].taskIds[j], requestToString(req));
			}

			return;
		}
	}
}


bool masterCalculationBehaviour(Request request, Response * response){

	//reset the response
	memset(response,0,sizeof(Response));
	response->taskId = ID;

	//Loop until password solved or a new response recived
	do{
		//increment the try number
		(response->ntries)++;

		//do the calculus and then check if the solution has been found
		if(doCalculus(&(request.p),request.rangeMin,request.rangeMax)){
			//fill the response
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

	do{
		//Reset the request and response, and wait to password request
		memset(&response,0,sizeof(Response));
		memset(&request,0,sizeof(Request));
		recv(MASTER_ID, &request, MPI_REQUEST_STRUCT(request), MPI_ANY_TAG);

		//Loop until password solved or a new order recived
		do{
			//increment the try number
			response.ntries++;

			//do the calculus and then check if the solution has been found
			if(doCalculus(&(request.p),request.rangeMin,request.rangeMax)){
				//fill the response and send to master
				response.taskId = ID;
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
