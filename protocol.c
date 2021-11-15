
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>

#define TRASMITTER 0
#define RECEIVER 1

#define FLAG 0x7E
#define SUPERVISION_FRAMESIZE 5
//COMANDOS SET E DISC
#define SET 0x03
#define DISC 0x11


//RESPOSTAS UA RR REJ
#define UA 0x07
#define RR0 2
#define RR1 2
#define REJ0 3
#define REJ1 3

struct linkLayer {
    char port[20]; /*Dispositivo /dev/ttySx, x = 0, 1*/
    int baudRate; /*Velocidade de transmissão*/
    unsigned int sequenceNumber;   /*Número de sequência da trama: 0, 1*/
    unsigned int timeout; /*Valor do temporizador: 1 s*/
    unsigned int numTransmissions; /*Número de tentativas em caso de falha*/
    char frame[MAX_SIZE]; /*Trama*/
}

struct linkLayer l1;

int createSuperVisionFrame (int option, int controlField){
    l1.frame[0] = FLAG;
    if(option == TRASMITTER){
        if (controlField == (SET || DISC)) l1.frame[1]=0x03; 
        else if(controlField == (UA || RR0 || RR1 || REJ0 || REJ1 )) l1.frame[1]=0x01;
    }
    else if(option == RECEIVER){
        if (controlField == (SET || DISC)) l1.frame[1]=0x01; 
        else if(controlField == (UA || RR0 || RR1 || REJ0 || REJ1 )) l1.frame[1]=0x03;
    }
    l1.frame[2]=controlField;
    l1.frame[3]=l1.frame[1]^l1.frame[2];
    l1.frame[4]=FLAG;
}

int sendFrame(int fd, int frameSize){
    for (int i=0;i <= frameSize;i++){
        write(fd,&l1.frame[i],1);
    }  
}

int llopen(int port, int option){
    int fd;
    struct termios oldtio,newtio;
    fd = open(port, O_RDWR | O_NOCTTY );
    if (fd <0) {
        perror(port);
        exit(-1); 
    }
    if(option == TRASMITTER){
        createSuperVisionFrame(option,SET);
        sendFrame(fd, SUPERVISION_FRAMESIZE);
    }

    return 1;
}
