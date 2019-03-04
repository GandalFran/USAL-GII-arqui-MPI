#ifndef __TOSTRING_H
#define __TOSTRING_H

#include <stdlib.h>
#include <string.h>
#include "dataDefinition.h"

char * passwordToString(Password p);
char * requestToString(Request req);
char * responseToString(Response res);
char * passwordStatusToString(PasswordStatus ps);
char * messageTagToSring(MessageTag msg);

#endif