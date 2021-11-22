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
    unsigned char port[20];                 /*Dispositivo /dev/ttySx, x = 0, 1*/
    int baudRate;                  /*Velocidade de transmissão*/
    unsigned int sequenceNumber;   /*Número de sequência da trama: 0, 1*/
    unsigned int timeout;          /*Valor do temporizador }: 1 s*/
    unsigned int numTransmissions; /*Número de tentativas em caso de falha*/
    struct termios oldtio, newtio;
    unsigned char frame[CHAR_MAX]; /*Trama*/
};

struct linkLayer l1;

enum state { START, FLAG_RCV, A_RCV, C_RCV, BCC_OK, STOP};

int flag = 1, try = 1;

void alarmHandler();

int createSuperVisionFrame(int user, unsigned char controlField, unsigned char *frame);

int sendSupervisionFrame(int fd, int user, unsigned char controlField, unsigned char responseControlField);

int receiveSupervisionFrame(int fd, unsigned char expectedControlField, unsigned char responseControlField);

int llopen(unsigned char *port, int user);

int llclose(int fd, int user);


/**
 * Function that creates BCC2
 * @param dataSize Size of data that will be used to create BCC2
 * @return BCC2
 */

unsigned char createBCC2(int dataSize);

/**
 * Function does byte Stuffing to a frame
 * @param dataSize Size of data that will be used to create BCC2 + 1 (because BCC also needs stuffing)
 * @return BCC2
 */

int byteStuffing(int dataSize);

/**
 * Function that creates information frame
 * @param data Data that information frame will contain
 * @param dataSize Size of data that will be used to create BCC2
 * @return information frame size (after destuffing)
 */
int createInformationFrame(unsigned char *data, int dataSize);

/**
 * Function does byte Stuffing to a frame
 * @param buffedDataSize Size of data that will be used to create BCC2
 * @return BCC2
 */
int byteDestuffing(int buffedDataSize);


/**
 * Function does byte Stuffing to a frame
 * @param dataSize Size of data that will be used to create BCC2
 * @return BCC2
 */
enum state supervisionEventHandler(unsigned char byteRead, enum state st, unsigned char* supervisionFrame);

unsigned char readSupervisionFrame(int fd);

int llwrite(int fd, unsigned char * buffer, int length);

/**
 * Event handler to update the state according to the byteRead
 * @
 */ 
enum state informationEventHandler(unsigned char byteRead, enum state st, int *buffedFrameSize);

/**
 * Function that reads information frame
 * @param fd File descriptor of the serial port
 * @return buffedFrameSize : size of information frame read
 */ 
int readInformationFrame(int fd);

int checkBCC2(int numBytesAfterDestuffing);

/**
 * Function that returns the response byte to informe the transmitter that the I frame was sucessfull and is ready to read new frame
 * @param controlField information frame's control field
 * @return responseByte 
 */
unsigned char nextFrame(int controlField);

/**
 * Function that returns the response byte to informe the transmitter that the I frame needs to be resend
 * @param controlField information frame's control field
 * @return responseByte 
 */
unsigned char resendFrame(int controlField);

/**
 * Function that checks if Trama read by receiver is new or was already read 
 * @param controlField information frame's control field
 * @return TRUE if duplicated FALSE if new and -1 if error occured 
 */
int checkDuplicatedFrame(int controlField);

/**
 * Auxiliar function that returns the information frame's control field
 * @return controlField
 */
int getSequenceNumber();

/**
 * Function that sends supervision frame to trasmitter after reading information frame
 * @param fd File descriptor of the serial port
 * @param buffer Array of characters where the read information will be stored
 * @param responseField response byte that will inform the transmitter if frame was sucessfully read or not
 */
int saveFrameInBuffer(unsigned char *buffer, int numBytesAfterDestuffing);

/**
 * Function that sends supervision frame to trasmitter after reading information frame
 * @param fd File descriptor of the serial port
 * @param responseField response byte that will inform the transmitter if frame was sucessfully read or not
 */
int sendConfirmation(int fd, unsigned char responseField);


/**
 * Function that reads the information written in the serial port
 * @param fd File descriptor of the serial port
 * @param buffer Array of characters where the read information will be stored
 * @return Number of characters read; -1 in case of error
 */
int llread(int fd, unsigned char *buffer);

