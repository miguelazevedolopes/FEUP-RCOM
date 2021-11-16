
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>


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

#define NUMBER_ATTEMPTS 3
#define ALARM_WAIT_TIME 3

char createBCC2(char *data, int size){

    char bcc2 = data[0];

    for(int i = 1; i < size; i++){
        bcc2 = bcc2 ^ data[i];
    }
    return bcc2;
}


int createInformationFrame(unsigned char* frame, unsigned char controlField, unsigned char* infoField, int infoFieldLength) {

  frame[0] = FLAG;

  frame[1] = END_SEND; // como so o emissor envia tramas I, assume-se que o campo de endereco e sempre 0x03.

  frame[2] = controlField;

  frame[3] = createBCC(frame[1], frame[2]);

  for(int i = 0; i < infoFieldLength; i++) {
    frame[i + 4] = infoField[i];
  }

  unsigned bcc2 = createBCC_2(infoField, infoFieldLength);

  frame[infoFieldLength + 4] = bcc2;

  frame[infoFieldLength + 5] = FLAG;

  return 0;
}


//Create Information Frame
int createInformationFrame (char controlField, char* data, int dataSize){
    l1.frame[0] = FLAG;
    l1.frame[1] = 0x03; //valor fixo pq só emissor envia I e I é um comando
    l1.frame[2] = controlField;
    for(int i = 0; i < dataSize;i++){
        frame
    }
    l1.frame[3]=createBCC2(data,dataSize);
    l1.frame[4]=FLAG;
}

int main(int argc, char** argv){
    char oi = createBCC2("100",2);
}
