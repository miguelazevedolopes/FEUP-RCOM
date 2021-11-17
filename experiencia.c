
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <limits.h>


#define FALSE 0
#define TRUE 1

#define TRASMITTER 0
#define RECEIVER 1

#define FLAG 0x7E
#define SUPERVISION_FRAME_SIZE 5
//COMANDOS SET E DISC
#define SET 0x03
#define DISC 0x11


//RESPOSTAS UA RR REJ
#define UA 0x07
#define RR0 2
#define RR1 2
#define REJ0 3
#define REJ1 3
#define NONE 0xFF

#define NUMBER_ATTEMPTS 3
#define ALARM_WAIT_TIME 3

#define ESCAPE 0x7D
#define DATA_START

struct linkLayer {
    char port[20]; /*Dispositivo /dev/ttySx, x = 0, 1*/
    int baudRate; /*Velocidade de transmissão*/
    unsigned int sequenceNumber;   /*Número de sequência da trama: 0, 1*/
    unsigned int timeout; /*Valor do temporizador }: 1 s*/
    unsigned int numTransmissions; /*Número de tentativas em caso de falha*/
    char frame[CHAR_MAX]; /*Trama*/
};

struct linkLayer l1;

char createBCC2(char *data, int size){

    char bcc2 = data[0];

    for(int i = 1; i < size; i++){
        bcc2 = bcc2 ^ data[i];
    }
    return bcc2;
}

// uma trama pode ser iniciada por uma ou mais flags
/*
int numberFlags(){
    l1.l
}
*/

//Create Information Frame
int createInformationFrame (char controlField, char* data, int dataSize){
    l1.frame[0] = FLAG;
    l1.frame[1] = 0x03; //valor fixo pq só emissor envia I e I é um comando
    l1.frame[2] = controlField;
    for(int i = 0; i < dataSize;i++){
        l1.frame[i] = data[i];
        printf("%x", l1.frame[i]);
    }
    l1.frame[3]=createBCC2(data,dataSize);
    l1.frame[4]=FLAG;

    return 0;
}


//PROBLEMA AINDA ESTOU A IGNORAR A FLAG INICIAL E O FINAL PQ SÓ É PARA FAZER BYTE STUFFING AO
int byteStuffing(char *data, int dataSize){
    beforeStuffingFrame[dataSize];

    for(int i = DATA_START; i < dataSize; i++){
        beforeStuffingFrame[i] = l1.frame[i];
    }
    
    for(int i = 0; i < dataSize; i++){
        if(beforeStuffingFrame[i] == FLAG){
            l1.frame[i] = ESCAPE;
            l1.frame[i+1] = 0x5E;
            i += 2;
        }
        else if(beforeStuffingFrame[i]== ESCAPE){
            l1.frame[i] = ESCAPE;
            l1.frame[i+1] = 0x5D;
            i += 2;
        }
        else{
            l1.frame[i] = beforeStuffingFrame[i];
        }
    }
    return 0;
}

int llwrite(int fd, char * buffer, int length){
    int informationFrame;
    informationFrame = createInformationFrame();
    char controlField; 
    if(l1.sequenceNumber == 0) controlField = 0x00; //00000000
    else controlField = 0x40; //01000000
    if(createInformationFrame(controlField) != 0) return 1;
    if(byteStuffing(data, dataSize) != 0) return 1;

}

int main(int argc, char** argv){
    int sofi;
    sofi = createInformationFrame (char controlField, char* data, int dataSize);
}

/*
TO DO :
- Byte Stuffing só do data
- Descobrir se eles verificam se a trama pode ser iniciada  por maias do que uma flag
- DÚVIDA : se sou eu que crio a information frame como é que pode haver mais do que uma flag a inicializa la
*/
