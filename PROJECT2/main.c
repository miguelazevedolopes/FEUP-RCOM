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

#include "download.h"

int main(int argc, char *argv[])
{
    if (argc != 2) {
        printf("Usage: %s %s\n", argv[0], "ftp://[<user>:<password>@]<host>/<url-path>");
        return -1;
    }
    arguments args;
    if(parseArgument(argv[1],&args)<0){
        printf("Error parsing arguments.\n");
        printf("Usage: %s %s\n", argv[0], "ftp://[<user>:<password>@]<host>/<url-path>");
        exit(-1);
    };

    if(transferFile(&args) != 0) return -1;
    return 0;
}