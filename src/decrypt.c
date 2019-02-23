
#include "header.h"

//Task definition
void communicationTask();
void calculationTask(CalculationMode m, Password p);

//Calculus definition
bool doCalculus(Password * p, SaltPointer salt);

//Wrapping and treatment of MPI library
int getId();
int getNtasks();


// -------------------------------- Main --------------------------------

int main (int argc, char * argv[]){
	Password p;

	//initialize the MPI
	MPI_Init(&argc, &argv);
	LOG("\n[ID:%d] Started",ID);

	//Taking each one to his role
	if(IS_MASTER(ID)){
		communicationTask();
	}else{
		//here it is not neccesary to send a p
		calculationTask(p);
	}

	//Finalize execution
	MPI_EXIT(EXIT_SUCCESS);
}


// -------------------------------- Task definition --------------------------------
void communicationTask(){
	Salt salt;
	int i;
	Solution solutionList[MAX_TASKS]; 
	Password passwordList[MAX_TASKS];
	bool isPasswordDecrypted[MAX_TASKS];

	//reset the solutionList and passwordList
	memset(solutionList,0,MAX_TASKS*sizeof(Solution));
	memset(passwordList,0,MAX_TASKS*sizeof(Password));
	memset(isPasswordDecrypted,0,MAX_TASKS*sizeof(bool));

	//generate passwords
	for(i=0; i<NTASKS; i++){
		GET_RANDMON_SALT(salt);
		GET_RANDOM_STR_IN_BOUNDS(passwordList[i].decrypted,0,MAX_RAND);
		strcpy(passwordList[i].encrypted, crypt(passwordList[i].decrypted,salt));
		LOG("\n[ID:%d][Generated] %s",ID,passwordToString(passwordList[i]));
	}

	notSolvedPasswords = NTASKS;
	do{
		//Division of tasks
		doTaskDivision(passwordList,solutionList,isPasswordDecrypted);

		//go to the calculation -> there a REQUEST for calculus will be recived
		masterCalculationTask();

		//recv

		//response gestion

		if(false /*The father is the one who solved, stop the processes which are solving the same*/){
			//stop the child process which is processing the remaining password
		}	

		notSolvedPasswords--;
	}while(notSolvedPasswords > 0);

}

void calculationTask(){
	Salt salt;
	Request request;
	Response response;
	bool recivedFinalizeOrder = FALSE;

	//reset the response
	memset(response,0,sizeof(Response));
	response.id = ID;

	do{

		//Reset the request and wait to password request or finalize
		memset(request,0,sizeof(Request));
		//recv
		LOG("\n[ID %d][Recived] %s",ID,requestToString(request));

		//obtain the salt
		GET_SALT(satl,request.p.encrypted);

		do{
			//increment the try number
			response.ntries++;

			//do the calculus and then check if the solution has been found
			if(doCalculus(&p,salt)){
				solutionFound = TRUE;

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
	memset(response,0,sizeof(Response));
	response.id = ID;

	//Reset the request and wait to password request or finalize
	memset(request,0,sizeof(Request));
	//recv
	LOG("\n[ID %d][Recived] %s",ID,requestToString(request));

	//obtain the salt
	GET_SALT(satl,request.p.encrypted);

	do{
		//increment the try number
		response.ntries++;

		//do the calculus and then check if the solution has been found
		if(doCalculus(&p,salt)){
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

void doTaskDivision(Password * passwordList, Solution * solutionList, bool * isPasswordDecrypted){
	int i;

	for(i=0; i < NTASKS; i++){
		if( !isPasswordDecrypted[i] ){
				LOG("\n[ID:%d] Entering in calculation of %d",ID,i);
				calculationTask(passwordList[i]);
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
		 ntasks = MPI_Comm_rank();
	
	return ntsks;
}

//others

char * passwordToString(Password p){
	static char tag[TAG_SIZE];

	memset(tag,0,TAG_SIZE);
	sprintf("Password{decrypted:%s, encrypted: %s, salt: %c%c}",p.decrypted,p.encrypted,p.encrypted[0],p.encrypted[1]);

	return tag;
}

char * requestToString(Request req){
	static char tag[TAG_SIZE];

	memset(tag,0,TAG_SIZE);
	sprintf("Request{password:%s }",passwordToString(req.p));

	return tag;
}

char * responseToString(Response res){
	static char tag[TAG_SIZE];

	memset(tag,0,TAG_SIZE);
	sprintf("Response{id:%d, ntries: %d, password:%s }",res.id,res.ntries,passwordToString(res.p));

	return tag;
}