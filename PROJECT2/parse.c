#include "parse.h"

int hasUser(char * args) {
    return strchr(args, '@') != NULL ? 1 : 0;
}

int parseArgument(char *arg, arguments *parsedArguments) {
    char * ftp = strtok(arg, "/");
    char * args = strtok(NULL, "/");
    char * path = strtok(NULL, "");

    if (ftp == NULL || args == NULL || path == NULL) {
        fprintf(stderr, "Invalid URL!\n");
        return -1;
    }

    if (hasUser(args)) {
        char * login = strtok(args, "@");
        char * host = strtok(NULL, "@");

        char * user = strtok(login, ":");
        char * password = strtok(NULL, ":");

        strcpy(parsedArguments->user, user);

        if (password == NULL)
            strcpy(parsedArguments->password, "pass");
        else
            strcpy(parsedArguments->password, password);

        strcpy(parsedArguments->host, host);
    }
    else {
        strcpy(parsedArguments->user, "anonymous");
        strcpy(parsedArguments->password, "pass");
        strcpy(parsedArguments->host, args);
    }

    strcpy(parsedArguments->urlPath, path);

    if (!strcmp(parsedArguments->host, "") || !strcmp(parsedArguments->urlPath, "")) {
        fprintf(stderr, "Invalid URL!\n");
        return -1;
    }

    struct hostent * h;

    if ((h = gethostbyname(parsedArguments->host)) == NULL) {  
        herror("gethostbyname");
        return -1;
    }

    char * ip = inet_ntoa(*((struct in_addr *)h->h_addr));
    strcpy(parsedArguments->ip, ip);

    printf("\nUser: %s\n", parsedArguments->user);
    printf("Password: %s\n", parsedArguments->password);
    printf("Host: %s\n", parsedArguments->host);
    printf("Path: %s\n", parsedArguments->urlPath);
    printf("IP Address : %s\n\n", parsedArguments->ip);

    return 0;
}