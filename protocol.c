#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <limits.h>
#include "macros.h"

struct linkLayer
{
    char port[20];                 /*Dispositivo /dev/ttySx, x = 0, 1*/
    int baudRate;                  /*Velocidade de transmissão*/
    unsigned int sequenceNumber;   /*Número de sequência da trama: 0, 1*/
    unsigned int timeout;          /*Valor do temporizador }: 1 s*/
    unsigned int numTransmissions; /*Número de tentativas em caso de falha*/
    struct termios oldtio, newtio;
    char frame[CHAR_MAX]; /*Trama*/
};

struct linkLayer l1;

int flag = 1, try = 1;

void alarmHandler() // atende alarme
{
    printf("Sending again. Try: # %d\n", try);
    flag = 1;
    try++;
}

int createSuperVisionFrame(int user, int controlField, char *frame)
{
    frame[0] = FLAG;
    if (user == TRANSMITTER)
    {
        if (controlField == SET || controlField == DISC)
        {
            frame[1] = 0x03;
        }
        else if (controlField == UA)
            frame[1] = 0x01;
    }
    else if (user == RECEIVER)
    {
        if (controlField == DISC)
            frame[1] = 0x01;
        else if (controlField == (UA || RR0 || RR1 || REJ0 || REJ1))
            frame[1] = 0x03;
    }
    frame[2] = controlField;
    frame[3] = frame[1] ^ frame[2];
    frame[4] = FLAG;
}

int sendSupervisionFrame(int fd, int user, int controlField, int responseControlField)
{

    char frameToSend[SUPERVISION_FRAME_SIZE];
    char responseFrame[SUPERVISION_FRAME_SIZE];

    if (user == TRANSMITTER)
    {
        createSuperVisionFrame(TRANSMITTER, controlField, frameToSend); //Creates the frame to send
        createSuperVisionFrame(RECEIVER, responseControlField, responseFrame);
    }
    else if (user == RECEIVER)
    {
        createSuperVisionFrame(RECEIVER, controlField, frameToSend); //Creates the frame to send
        createSuperVisionFrame(TRANSMITTER, responseControlField, responseFrame);
    }

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
            for (int i = 0; i < SUPERVISION_FRAME_SIZE; i++)
            {
                write(fd, &(frameToSend[i]), 1);
                printf("%x\n", frameToSend[i]);
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
            if (responseBuffer[0] == responseFrame[0])
            {
                FLAG_RCV = TRUE;
                if (A_RCV && C_RCV && BCC_OK && FLAG_RCV)
                {
                    STOP = TRUE;
                }
                printf("%x\n", responseBuffer[0]);
            }
            else if (responseBuffer[0] == responseFrame[1])
            {
                if (FLAG_RCV && !A_RCV)
                    A_RCV = TRUE;
                printf("%x\n", responseBuffer[0]);
            }
            else if (responseBuffer[0] == responseFrame[2])
            {
                if (FLAG_RCV && A_RCV)
                    C_RCV = TRUE;
            }
            else if (responseBuffer[0] == 0x03 ^ responseFrame[3])
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

int receiveSupervisionFrame(int fd, int expectedControlField, int responseControlField)
{

    char frameToReceive[SUPERVISION_FRAME_SIZE];
    char frameToSend[SUPERVISION_FRAME_SIZE];

    createSuperVisionFrame(TRANSMITTER, expectedControlField, frameToReceive);

    createSuperVisionFrame(RECEIVER, responseControlField, frameToSend);

    int FLAG_RCV = FALSE;
    int A_RCV = FALSE;
    int C_RCV = FALSE;
    int BCC_OK = FALSE;
    int STOP = FALSE;

    char responseBuffer[5];
    while (!STOP)
    {
        read(fd, responseBuffer, 1);

        if (responseBuffer[0] == frameToReceive[0])
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
        else if (responseBuffer[0] == frameToReceive[1])
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
        else if (responseBuffer[0] == frameToReceive[3])
        {
            if (FLAG_RCV && A_RCV && C_RCV)
            {
                BCC_OK = TRUE;
                printf("BCC: %x\n", responseBuffer[0]);
            }
        }
    }
    for (int i = 0; i < SUPERVISION_FRAME_SIZE; i++)
    {
        write(fd, &(frameToSend[i]), 1);
        printf("%x\n", frameToSend[i]);
    }
}

int llopen(char *port, int user)
{
    int fd;

    fd = open(port, O_RDWR | O_NOCTTY);
    if (fd < 0)
    {
        perror(port);
        exit(-1);
    }
    if (tcgetattr(fd, &l1.oldtio) == -1)
    { /* save current port settings */
        perror("tcgetattr");
        exit(-1);
    }

    bzero(&l1.newtio, sizeof(l1.newtio));
    l1.newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    l1.newtio.c_iflag = IGNPAR;
    l1.newtio.c_oflag = 0;

    /* set input mode (non-canonical, no echo,...) */
    l1.newtio.c_lflag = 0;

    if (user == TRANSMITTER)
    {

        l1.newtio.c_cc[VTIME] = 0; /* inter-character timer unused */
        l1.newtio.c_cc[VMIN] = 0;  /* blocking read until 5 chars received */

        tcflush(fd, TCIOFLUSH);

        if (tcsetattr(fd, TCSANOW, &l1.newtio) == -1)
        {
            perror("tcsetattr");
            exit(-1);
        }

        (void)signal(SIGALRM, alarmHandler); //Alarm setup

        if (sendSupervisionFrame(fd, TRANSMITTER, SET, UA) < 0)
        { //Sends the frame to the receiver
            printf("No response received. Gave up after %d tries", NUMBER_ATTEMPTS);
            exit(1);
        }
    }
    else if (user == RECEIVER)
    {
        l1.newtio.c_cc[VTIME] = 0; /* inter-character timer unused */
        l1.newtio.c_cc[VMIN] = 1;  /* blocking read until 5 chars received */

        tcflush(fd, TCIOFLUSH);

        if (tcsetattr(fd, TCSANOW, &l1.newtio) == -1)
        {
            perror("tcsetattr");
            exit(-1);
        }
        receiveSupervisionFrame(fd, SET, UA);
    }

    return 0;
}

int llclose(int fd, int user)
{

    if (user == TRANSMITTER)
    {

        (void)signal(SIGALRM, alarmHandler); //Alarm setup

        if (sendSupervisionFrame(fd, TRANSMITTER, DISC, DISC) < 0)
        { //Sends the frame to the receiver
            printf("No response received. Gave up after %d tries", NUMBER_ATTEMPTS);
            exit(1);
        }
        receiveSupervisionFrame(fd, DISC, UA);
    }
    else if (user == RECEIVER)
    {
        receiveSupervisionFrame(fd, DISC, DISC);
    }
}

char createBCC2(int dataSize)
{

    int bcc2 = l1.frame[DATA_START];

    for (int i = 1; i < dataSize; i++)
    {
        bcc2 = bcc2 ^ l1.frame[DATA_START + i];
    }

    return bcc2;
}

int byteStuffing(int dataSize)
{
    int buffedDataSize = dataSize;
    char beforeStuffingFrame[dataSize];

    for (int i = DATA_START; i < (DATA_START + dataSize); i++)
    {
        beforeStuffingFrame[i] = l1.frame[i];
    }

    int counter = 0;
    int indexAfterStuffing = DATA_START;
    int indexBeforeStuffing = DATA_START;
    while (counter < (dataSize * 2))
    {
        if (beforeStuffingFrame[indexBeforeStuffing] == FLAG)
        {
            l1.frame[indexAfterStuffing] = ESCAPE;
            l1.frame[indexAfterStuffing + 1] = 0x5E;
            buffedDataSize += 1;
            indexAfterStuffing += 2;
        }
        else if (beforeStuffingFrame[indexBeforeStuffing] == ESCAPE)
        {
            l1.frame[indexAfterStuffing] = ESCAPE;
            l1.frame[indexAfterStuffing + 1] = 0x5D;
            buffedDataSize += 1;
            indexAfterStuffing += 2;
        }
        else
        {
            l1.frame[indexAfterStuffing] = beforeStuffingFrame[indexBeforeStuffing];
            indexAfterStuffing += 1;
        }
        counter += 2;
        indexBeforeStuffing += 1;
    }

    return buffedDataSize;
}

//Create Information Frame
int createInformationFrame(unsigned char *data, int dataSize)
{
    l1.frame[0] = FLAG;
    l1.frame[1] = 0x03; //valor fixo pq só emissor envia I e I é um comando

    //control field
    if (l1.sequenceNumber == 0)
        l1.frame[2] = CONTROL_FIELD_O;
    else
        l1.frame[2] = CONTROL_FIELD_1;

    char bcc2 = createBCC2(dataSize);

    for (int i = 0; i < dataSize; i++)
    {
        l1.frame[DATA_START + i] = data[i];
    }

    l1.frame[DATA_START + dataSize] = bcc2;

    int buffedDataSize = byteStuffing(dataSize + 1); //bcc also needs stuffing

    l1.frame[DATA_START + buffedDataSize] = FLAG;

    return DATA_START + buffedDataSize + 1;
}

int byteDestuffing(int buffedDataSize)
{
    char beforeDestuffingFrame[buffedDataSize];

    for (int i = DATA_START; i < (DATA_START + buffedDataSize); i++)
    {
        beforeDestuffingFrame[i] = l1.frame[i];
    }

    int indexAfterDestuffing = DATA_START;
    int indexBeforeDeStuffing = DATA_START;

    while (indexBeforeDeStuffing < (buffedDataSize - 1))
    {
        if (beforeDestuffingFrame[indexBeforeDeStuffing] == ESCAPE)
        {
            if (beforeDestuffingFrame[indexBeforeDeStuffing + 1] == 0x5E)
            {
                l1.frame[indexAfterDestuffing] = FLAG;
                indexBeforeDeStuffing += 2;
            }
            else if (beforeDestuffingFrame[indexBeforeDeStuffing + 1] == 0x5D)
            {
                l1.frame[indexAfterDestuffing] = ESCAPE;
                indexBeforeDeStuffing += 2;
            }
        }
        else
        {
            l1.frame[indexAfterDestuffing] = beforeDestuffingFrame[indexBeforeDeStuffing];
            indexBeforeDeStuffing++;
        }
        indexAfterDestuffing++;
    }

    l1.frame[indexAfterDestuffing] = 0x7E; //restore flag value
    return 0;
}

/*
int sendInformationFrame(char *frame, int fd, int length){
    int n;
    
    if((n = write(fd, frame, length)) <= 0) return -1;

    return n;
}*/

int llwrite(int fd, char *buffer, int length)
{
    unsigned char miguel[255];
    miguel[0] = ESCAPE;
    miguel[1] = FLAG;

    int frameSize = createInformationFrame(miguel, 2);

    //write(fd, l1.frame, length)) <= 0 // send Information Frame

    alarm(ALARM_WAIT_TIME); //temporizador ativado após o envio

    int counter = 0;
    int numWrittenBytes = 0;
    while (counter < NUMBER_ATTEMPTS)
    {
        //WRITE INFORMATION FRAMEvou

        counter++;
    }
    /*int validResponse = 0;

    if(validResponse) alarm(0); //deasita temporador após resposta válida

    char response[2];

    //l1.frame[2] = control field
    if(l1.frame[2] == CONTROL_FIELD_O){
        response[0] = RR1;
        response[1] = REJ0;
    }
    else if (l1.frame[2] == CONTROL_FIELD_1){
        response[0] == RR0;
        response[1] = REJ1;
    }*/

    if (l1.sequenceNumber == 0)
        l1.sequenceNumber = 1;
    else if (l1.sequenceNumber == 1)
        l1.sequenceNumber = 0;
    else
        return -1;
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

    llopen(argv[1], TRANSMITTER);

    //if(byteDestuffing(9) != 0) printf("merda \n");
}

/*
FALTA:
- fazer bite stuffing ao bcc 
- ter o bcc direito (aquilo do stuff antes e depois)
- depois de fazer destuffing tenho de por o "excesso" a 0 ou cago só pq tem a flag final?
- confirmar se o cálculo de bcc está certo
- a confirmação de tramas repetidas é feita em que parte?
*/