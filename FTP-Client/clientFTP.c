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

int getIP(char* ip,char* hostName){
    struct hostent *h;
    if ((h = gethostbyname(hostName)) == NULL) {
        herror("gethostbyname()");
        exit(-1);
    }
    strcpy(ip, inet_ntoa(*((struct in_addr *)h->h_addr)));
    return 0;
}

int parseArgument(char *arg, arguments *parsedArguments){
    if(strlen(arg)<6) return -1;
    char checkValidity[7];
    memcpy( checkValidity, &arg[0], 6 );
    checkValidity[6] = '\0';
    if(strcmp(checkValidity,"ftp://")!=0) return -1;
    int reference;
    for(int i=6;i<strlen(arg);i++){
        if(arg[i]=='@'){
            char aux[i-6];
            memcpy(aux, &arg[6],i-6);
            aux[i-6]='\0';
            char* token;
            token=strtok(aux,":");
            memcpy(parsedArguments->user,token,strlen(token));
            parsedArguments->user[strlen(token)]='\0';
            token=strtok(NULL,":");
            if(token==NULL) return -1;
            memcpy(parsedArguments->password,token,strlen(token));
            parsedArguments->password[strlen(token)]='\0';
            reference=i+1;
        }
        if(arg[i]=='/'){
            memcpy(parsedArguments->host, &arg[reference],i-reference);
            parsedArguments->host[i-reference]='\0';
            memcpy(parsedArguments->urlPath, &arg[i],strlen(arg)-i);
            parsedArguments->urlPath[strlen(arg)-i]='\0';
            break;
        }
    }
    return 0;
}

int socketCreateConnect(char *ipAddress){
    int sockfd;
    struct sockaddr_in server_addr;

    /*server address handling*/
    bzero((char *) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ipAddress);    /*32 bit Internet address network byte ordered*/
    server_addr.sin_port = htons(FTP_PORT);        /*server TCP port must be network byte ordered */

    /*open a TCP socket*/
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket()");
        exit(-1);
    }
    /*connect to the server*/
    if (connect(sockfd,
                (struct sockaddr *) &server_addr,
                sizeof(server_addr)) < 0) {
        perror("connect()");
        exit(-1);
    }

    return sockfd;
}

int createSocket(){
    int fd;

    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket()");
        exit(-1);
    }
    return fd;
}

int writeToSocket(int sockfd, char *message){
    int bytes;
    int messageSize = strlen(message);

    if((bytes = write(sockfd, message, messageSize)) != messageSize){
        fprintf(stderr, "Error writing message to socket!\n");
        return -1;
    }
    return 0;
    
}

int readFromSocket(int sockfd){
    FILE* fp = fdopen(sockfd, "r");

	char* buf;
	size_t bytesRead = 0;
	int response_code;

	while(getline(&buf, &bytesRead, fp) > 0){
		printf("< %s", buf);
		if(buf[3] == ' '){
			sscanf(buf, "%d", &response_code);
			break;
		}
	}

	return response_code;
}

void readRSP (int sockfd, char* responseCode){
    char byte;
    int currentState = 0;
    int indexResponse = 0;
    int multipleLine= 0; //If the response has multiple lines
    
    while(currentState != 2){
        read(sockfd, &byte, 1);
        
    }
}



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


    printf("Username: %s\n",args.user);
    printf("Password: %s\n",args.password);
    printf("Host: %s\n",args.host);
    printf("URL Path: %s\n",args.urlPath);

    char ipAddress[1024];
    if(getIP(ipAddress,args.host)<0){
        printf("Error obtaining IP address.\n");
        exit(-1);
    };
    printf("Obtained IP from host: %s\n",ipAddress);

    int controlSocket;
    //Creates control socket
    controlSocket=socketCreateConnect(ipAddress);

    return 0;
}
