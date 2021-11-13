/*Non-Canonical Input Processing*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>

#define BAUDRATE B38400
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

#define FLAG 0x7E; //flag
#define A 0x03; // campo de endere�o no emissor
#define SET_C 0x03;
#define UA_C 0x05


int main(int argc, char** argv)
{
  int fd,c, res;
  struct termios oldtio,newtio;
  char buf[255];

  if ( (argc < 2) || 
        ((strcmp("/dev/ttyS0", argv[1])!=0) && 
        (strcmp("/dev/ttyS1", argv[1])!=0) )) {
    printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
    exit(1);
  }


  /*
    Open serial port device for reading and writing and not as controlling tty
    because we don't want to get killed if linenoise sends CTRL-C.
  */
  
    
  fd = open(argv[1], O_RDWR | O_NOCTTY );
  if (fd <0) {perror(argv[1]); exit(-1); }

  if ( tcgetattr(fd,&oldtio) == -1) { /* save current port settings */
    perror("tcgetattr");
    exit(-1);
  }

  bzero(&newtio, sizeof(newtio));
  newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
  newtio.c_iflag = IGNPAR;
  newtio.c_oflag = 0;

  /* set input mode (non-canonical, no echo,...) */
  newtio.c_lflag = 0;

  newtio.c_cc[VTIME]    = 0;   /* inter-character timer unused */
  newtio.c_cc[VMIN]     = 1;   /* blocking read until 5 chars received */



  /* 
    VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a 
    leitura do(s) pr�ximo(s) caracter(es)
  */



  tcflush(fd, TCIOFLUSH);

  if ( tcsetattr(fd,TCSANOW,&newtio) == -1) {
    perror("tcsetattr");
    exit(-1);
  }

  printf("New termios structure set\n");


  volatile int STOP=FALSE;

  bool FLAG_RCV=FALSE;
  bool A_RCV=FALSE;
  bool C_RCV=FALSE;
  bool BCC_OK=FALSE;

  
  while (STOP==FALSE) {       /* loop for input */
    // res = read(fd,buf,5);   /* returns after 5 chars have been input */
    // 					/* so we can printf... */
    // //printf(":%s:%d\n", buf, res);
    // printf("%x\n",buf[0]);
    // printf("%x\n",buf[1]);
    // printf("%x\n",buf[2]);
    // printf("%x\n",buf[3]);
    // printf("%x\n",buf[4]);
    // if (buf[0] == F && buf[1] == A && buf[2] == 0x03 && buf[3] == (A^(0x03)) &&  buf[4] == F) STOP=TRUE;
    res = read(fd,buf,1);
    switch (buf)
    {
    case FLAG:
      FLAG_RCV=TRUE;
      if(A_RCV&&C_RCV&&BCC_OK&&FLAG_RCV){
        STOP=TRUE;
      }
      break;
    case A:
      if(FLAG_RCV)
        A_RCV=TRUE;
      break;
    case SET_C:
      if(FLAG_RCV&&A_RCV)
        A_RCV=TRUE;
      break;
    case (A^SET_C):
      if(FLAG_RCV&&A_RCV&&C_RCV){
        BCC_OK=TRUE;
      }
      break;
    default:
      break;
    }
  }
  
  

  unsigned char set [5];
  set[0] = FLAG;
  set[1] = A;
  set[2] = UA_C;
  set[3] = A^UA_C;
  set[4] = FLAG;

  write(fd,set,5);

  /* 
    O ciclo WHILE deve ser alterado de modo a respeitar o indicado no gui�o 
  */


	sleep(1);
  tcsetattr(fd,TCSANOW,&oldtio);
  close(fd);
  return 0;
}
