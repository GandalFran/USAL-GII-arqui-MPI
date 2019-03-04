#include "toString.h"

char * passwordToString(Password p){
	static char tag[TAG_SIZE];

	memset(tag,0,TAG_SIZE);
	sprintf(tag,"Password{id:%d, decrypted:%s, encrypted: %s, salt: %s}",p.passwordId,p.decrypted,p.encrypted,p.s);

	return tag;
}

char * requestToString(Request req){
	static char tag[TAG_SIZE];

	memset(tag,0,TAG_SIZE);
	sprintf(tag,"Request{rangeMin:%d, rangeMax:%d, passwordId:%d }",req.rangeMin,req.rangeMax,req.p.passwordId);

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
	sprintf(tag,"%s]}",tag);

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