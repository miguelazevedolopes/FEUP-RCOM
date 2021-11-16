
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <limits.h>

#define BAUDRATE B38400

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

int main(int argc, char **argv)
{
    if ((argc < 2) ||
        ((strcmp("/dev/ttyS10", argv[1]) != 0) &&
         (strcmp("/dev/ttyS11", argv[1]) != 0)))
    {
        printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
        exit(1);
    }

    llopen(argv[1], RECEIVER);
}