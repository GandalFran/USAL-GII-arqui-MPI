
#include "header.h"

//Task definition
void communicationTask();
void communicationAndCalculationTask(Password p);
void calculationTask();

//Calculus definition
bool doCalculus(Password * p, SaltPointer salt);

//Wrapping and treatment of MPI library
int getId();

// -------------------------------- Main --------------------------------

int main (int argc, char * argv[]){

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
	Salt salt;
	int i;
	Solution solutionList[N_PASSWORDS]; 
	Password passwordList[N_PASSWORDS];

	//reset the solutionList and passwordList
	memset(solutionList,0,N_PASSWORDS*sizeof(Solution));
	memset(passwordList,0,N_PASSWORDS*sizeof(Password));

	//generate and send the passwords
	for(i=0; i<N_PASSWORDS; i++){
		//generate password and encrypt
		GET_RANDMON_SALT(salt);
		GET_RANDOM_STR_IN_BOUNDS(passwordList[i].decrypted,0,MAX_RAND);

		strcpy(passwordList[i].encrypted, crypt(passwordList[i].decrypted,salt));

		//send the password to a process (temporary the i process)

		LOG("\n[ID:%d][Generated] %s Salt %s -> Encrypted %s -> send to %d",ID,passwordList[i].decrypted,salt,passwordList[i].decrypted,i);
	}

	//wait for all password except one
	for(i=0; i < N_PASSWORDS-1; i++){
		//blocking recv --> NOTE: for future work the main task should go to calculate each time it gives a password 

		LOG("\n[ID:%d][Recived] %s -> Encrypted %s -> recived by %d in %d tries",ID,solutionList[i].p.decrypted,solutionList[i].p.decrypted,solutionList[i].id, solutionList[i].ntries);
	}

	//prepare the wait for the remaining password --> it should be to declare a handler or something like that

	// then go to calculate the remaining password
	for(i=0; i < N_PASSWORDS; i++){
		if( !IS_MASTER(solutionList[i].id) ){
				LOG("\n[ID:%d] Entering in calculation",ID);
				communicationAndCalculationTask(passwordList[i]);
			break;
		}
	}

}

//same as calculation task, but with cheking of messages and password as parameter
void communicationAndCalculationTask(Password p){
	long ntries;
	bool solutionFound;
	Salt salt;

	//obtain the salt
	strncpy(salt,p.decrypted,2);

	solutionFound = FALSE;
	while(!solutionFound){
		//increment the try number
		ntries++;

		//to the calculus
		if(doCalculus(&p,salt)){
			solutionFound = TRUE;
			LOG("\n[ID:%d][NTRY:%ld] Found: %s -> %s ",ID,ntries,p.decrypted,p.encrypted);
		}

		//Non-blocking check if Master process has ended

	};

	//stop the child process which is processing the remaining password

	//print the solution

	//finalize
	MPI_EXIT(EXIT_SUCCESS);
}

void calculationTask(){
	Solution s;
	Password p;
	long ntries;
	bool solutionFound;
	Salt salt;

	//Wait to the fahter password


	//obtain the salt
	strncpy(salt,p.decrypted,2);

	solutionFound = FALSE;
	while(!solutionFound){
		//increment the try number
		ntries++;

		//do the calculus
		if(doCalculus(&p,salt)){
			solutionFound = TRUE;
			LOG("\n[ID:%d][NTRY:%ld] Found: %s -> %s ",ID,ntries,p.decrypted,p.encrypted);
		}

		//Non-blocking check if Master has ordered to stop (our password has been taken by master and decrypted)
		//	Note: if a message is recived, make an MPI_EXIT(EXIT_SUCCED)

	};

	//fill the solution structure
	s.id = ID;
	s.ntries = ntries;
	memcpy(&(s.p),&p,sizeof(Password));

	//send father process the possible solution

}

// -------------------------------- Calculus definition --------------------------------

bool doCalculus(Password * p, SaltPointer salt){	
	int randomNumber;
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

// -------------------------------- MPI --------------------------------

//Wrap to only get the Id one time
int getId(){
	static int internalId = -1;

	if(-1 == internalId)
		 MPI_Comm_rank(MPI_COMM_WORLD, &internalId);
	
	return internalId;
}
