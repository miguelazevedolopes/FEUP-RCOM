#ifndef PARCE_H
#define PARCE_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

#define FTP_PORT 21

typedef struct{
    char ip[1024];
    char host[1024];
    char urlPath[1024];
    char user[1024];
    char password[1024];
} arguments;

int parseArgument(char *arg, arguments *parsedArguments);

int hasUser(char * args);

#endif // DOWNLOAD_H