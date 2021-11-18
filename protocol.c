
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <limits.h>
#include "macros.h"

struct linkLayer
{
    char port[20];                 /*Dispositivo /dev/ttySx, x = 0, 1*/
    int baudRate;                  /*Velocidade de transmissão*/
    unsigned int sequenceNumber;   /*Número de sequência da trama: 0, 1*/
    unsigned int timeout;          /*Valor do temporizador
    }: 1 s*/
    unsigned int numTransmissions; /*Número de tentativas em caso de falha*/
    char frame[CHAR_MAX];          /*Trama*/
};

struct linkLayer l1;

int flag = 1, try = 1;

void alarmHandler() // atende alarme
{
    printf("Sending again. Try: # %d\n", try);
    flag = 1;
    try++;
}

//Creates Supervision and Unnumbered Frames
int createSuperVisionFrame(int user, int controlField)
{
    l1.frame[0] = FLAG;
    if (user == TRASMITTER)
    {
        if (controlField == SET || controlField == DISC)
        {
            l1.frame[1] = 0x03;
        }
        else if (controlField == (UA || RR0 || RR1 || REJ0 || REJ1))
            l1.frame[1] = 0x01;
    }
    else if (user == RECEIVER)
    {
        if (controlField == (SET || DISC))
            l1.frame[1] = 0x01;
        else if (controlField == (UA || RR0 || RR1 || REJ0 || REJ1))
            l1.frame[1] = 0x03;
    }
    l1.frame[2] = controlField;
    l1.frame[3] = l1.frame[1] ^ l1.frame[2];
    l1.frame[4] = FLAG;
}

int sendFrame(int fd, int frameSize, int responseControlField)
{
    char responseBuffer[5];

    int FLAG_RCV = FALSE;
    int A_RCV = FALSE;
    int C_RCV = FALSE;
    int BCC_OK = FALSE;
    int STOP = FALSE;
    while (!STOP && try <= NUMBER_ATTEMPTS)
    {
        if (flag)
        {
            for (int i = 0; i < frameSize; i++)
            {
                write(fd, &(l1.frame[i]), 1);
                printf("%x\n", l1.frame[i]);
            }
            printf("Escreveu uma vez!\n");
            alarm(ALARM_WAIT_TIME);
            flag = 0;
        }

        if (responseControlField == NONE)
        {
            STOP = TRUE;
        }
        else
        {
            read(fd, responseBuffer, 1);
            if (responseBuffer[0] == FLAG)
            {
                FLAG_RCV = TRUE;
                if (A_RCV && C_RCV && BCC_OK && FLAG_RCV)
                {
                    STOP = TRUE;
                }
                printf("%x\n", responseBuffer[0]);
            }
            else if (responseBuffer[0] == 0x03)
            {
                if (FLAG_RCV && !A_RCV)
                    A_RCV = TRUE;
                printf("%x\n", responseBuffer[0]);
            }
            else if (responseBuffer[0] == responseControlField)
            {
                if (FLAG_RCV && A_RCV)
                    C_RCV = TRUE;
            }
            else if (responseBuffer[0] == 0x03 ^ responseControlField)
            {
                if (FLAG_RCV && A_RCV && C_RCV)
                {
                    BCC_OK = TRUE;
                    printf("%x\n", responseBuffer[0]);
                }
            }
        }
    }
    if (try == NUMBER_ATTEMPTS)
        return -1;
    else
        return 0;
}

int receiveFrame(int fd, int controlField)
{

    int FLAG_RCV = FALSE;
    int A_RCV = FALSE;
    int C_RCV = FALSE;
    int BCC_OK = FALSE;
    int STOP = FALSE;

    char responseBuffer[5];
    while (!STOP)
    {
        read(fd, responseBuffer, 1);

        if (responseBuffer[0] == FLAG)
        {
            FLAG_RCV = TRUE;
            if (A_RCV && C_RCV && BCC_OK && FLAG_RCV)
            {
                STOP = TRUE;
                printf("%x\n", responseBuffer[0]);
                printf("ultima flag");
            }
            printf("Primeira flag\n");
        }
        else if (responseBuffer[0] == 0x03)
        {
            if (FLAG_RCV && !A_RCV)
            {
                A_RCV = TRUE;

                printf("A: %x\n", responseBuffer[0]);
            }
            else if (FLAG_RCV && A_RCV)
            {
                C_RCV = TRUE;
                printf("C: %x\n", responseBuffer[0]);
            }
        }
        else if (responseBuffer[0] == (0x03 ^ controlField))
        {
            if (FLAG_RCV && A_RCV && C_RCV)
            {
                BCC_OK = TRUE;
                printf("BCC: %x\n", responseBuffer[0]);
            }
        }
    }
}

int llopen(char *port, int user)
{
    int fd;
    struct termios oldtio, newtio;
    fd = open(port, O_RDWR | O_NOCTTY);
    if (fd < 0)
    {
        perror(port);
        exit(-1);
    }
    if (tcgetattr(fd, &oldtio) == -1)
    { /* save current port settings */
        perror("tcgetattr");
        exit(-1);
    }

    bzero(&newtio, sizeof(newtio));
    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    /* set input mode (non-canonical, no echo,...) */
    newtio.c_lflag = 0;

    if (user == TRASMITTER)
    {

        newtio.c_cc[VTIME] = 0; /* inter-character timer unused */
        newtio.c_cc[VMIN] = 0;  /* blocking read until 5 chars received */

        tcflush(fd, TCIOFLUSH);

        if (tcsetattr(fd, TCSANOW, &newtio) == -1)
        {
            perror("tcsetattr");
            exit(-1);
        }

        (void)signal(SIGALRM, alarmHandler); //Alarm setup

        createSuperVisionFrame(TRASMITTER, SET); //Creates the frame to send

        if (sendFrame(fd, SUPERVISION_FRAME_SIZE, UA) < 0)
        { //Sends the frame to the receiver
            printf("No response received. Gave up after %d tries", NUMBER_ATTEMPTS);
            exit(1);
        }
    }
    else if (user == RECEIVER)
    {
        newtio.c_cc[VTIME] = 0; /* inter-character timer unused */
        newtio.c_cc[VMIN] = 1;  /* blocking read until 5 chars received */

        tcflush(fd, TCIOFLUSH);

        if (tcsetattr(fd, TCSANOW, &newtio) == -1)
        {
            perror("tcsetattr");
            exit(-1);
        }

        createSuperVisionFrame(TRASMITTER, SET);
        receiveFrame(fd, SET);
        createSuperVisionFrame(RECEIVER, UA);
        sendFrame(fd, SUPERVISION_FRAME_SIZE, NONE);
    }

    return 0;
}

char createBCC2(int dataSize){

    int bcc2 = l1.frame[DATA_START];

    for(int i = 1; i < dataSize; i++){
        bcc2 = bcc2 ^ l1.frame[DATA_START + i];
    }

    return bcc2;
}

int byteStuffing(int dataSize){
    int buffedDataSize = dataSize;
    char beforeStuffingFrame[dataSize];

    for(int i = DATA_START; i < (DATA_START + dataSize); i++){
        beforeStuffingFrame[i] = l1.frame[i];
    }
  
    int counter = 0;
    int indexAfterStuffing = DATA_START;
    int indexBeforeStuffing = DATA_START;
    while(counter < (dataSize * 2)){
        if(beforeStuffingFrame[indexBeforeStuffing] == FLAG){
            l1.frame[indexAfterStuffing] = ESCAPE;
            l1.frame[indexAfterStuffing+1] = 0x5E;
            buffedDataSize += 1;
            indexAfterStuffing += 2;
        }
        else if(beforeStuffingFrame[indexBeforeStuffing]== ESCAPE){
            l1.frame[indexAfterStuffing] = ESCAPE;
            l1.frame[indexAfterStuffing+1] = 0x5D;
            buffedDataSize += 1;
            indexAfterStuffing += 2;
        }
        else{
            l1.frame[indexAfterStuffing] = beforeStuffingFrame[indexBeforeStuffing];
            indexAfterStuffing +=1 ;
        }
        counter += 2;
        indexBeforeStuffing += 1;

    }

    return buffedDataSize;
}

//Create Information Frame
int createInformationFrame (unsigned char* data, int dataSize){
    l1.frame[0] = FLAG;
    l1.frame[1] = 0x03; //valor fixo pq só emissor envia I e I é um comando
    
    //control field
    if(l1.sequenceNumber == 0) l1.frame[2] = 0x00; //00000000
    else l1.frame[2] = 0x40; //01000000

    char bcc2 = createBCC2(dataSize);

    for(int i = 0; i < dataSize;i++){
        l1.frame[DATA_START + i] = data[i];
    }

    l1.frame[DATA_START + dataSize] = bcc2;

    int buffedDataSize = byteStuffing(dataSize +1); //bcc also needs stuffing

    l1.frame[DATA_START +buffedDataSize] = FLAG;

    return DATA_START + buffedDataSize + 1;
}


int byteDestuffing(int buffedDataSize){
    char beforeDestuffingFrame[buffedDataSize];

    for(int i = DATA_START; i < (DATA_START + buffedDataSize); i++){
        beforeDestuffingFrame[i] = l1.frame[i];
    }
    
    int indexAfterDestuffing = DATA_START;
    int indexBeforeDeStuffing = DATA_START;

    while(indexBeforeDeStuffing < (buffedDataSize-1)){
        if(beforeDestuffingFrame[indexBeforeDeStuffing] == ESCAPE){
            if(beforeDestuffingFrame[indexBeforeDeStuffing + 1] == 0x5E){
                l1.frame[indexAfterDestuffing] = FLAG;
                indexBeforeDeStuffing += 2;
            }
            else if(beforeDestuffingFrame[indexBeforeDeStuffing + 1] == 0x5D){
                l1.frame[indexAfterDestuffing] = ESCAPE;
                indexBeforeDeStuffing += 2;
            }
        }
        else{
            l1.frame[indexAfterDestuffing] = beforeDestuffingFrame[indexBeforeDeStuffing];
            indexBeforeDeStuffing++;
        }
        indexAfterDestuffing++;
    }

    l1.frame[indexAfterDestuffing] = 0x7E; //restore flag value 
    return 0;
}

int llwrite(int fd, char * buffer, int length){
    unsigned char miguel [255];
    miguel[0] = ESCAPE;
    miguel[1] = FLAG;

    int frameSize = createInformationFrame(&miguel, 2);
    printf("frame size : %d \n", frameSize);

   // int sendFrame(fd, frameSize, int responseControlField)
    

}

int main(int argc, char **argv)
{
    if ((argc < 2) ||
        ((strcmp("/dev/ttyS10", argv[1]) != 0) &&
         (strcmp("/dev/ttyS11", argv[1]) != 0)))
    {
        printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
        exit(1);
    }

    unsigned char miguel [255];
    miguel[0] = ESCAPE;
    miguel[1] = FLAG;

    int frameSize = createInformationFrame(&miguel, 2);
    printf("frame size : %d \n", frameSize);

    //llopen(argv[1], RECEIVER);

    //if(byteDestuffing(9) != 0) printf("merda \n");
    
}

/*
FALTA:
- fazer bite stuffing ao bcc 
- ter o bcc direito (aquilo do stuff antes e depois)
- depois de fazer destuffing tenho de por o "excesso" a 0 ou cago só pq tem a flag final?
- confirmar se o cálculo de bcc está certo
*/