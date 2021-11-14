/*Non-Canonical Input Processing*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>

#define BAUDRATE B38400
#define MODEMDEVICE "/dev/ttyS1"
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

#define FLAG 0x7E //flag
#define A 0x03 // campo de endere�o no emissor
#define SET_C 0x03
#define UA_C 0x05
int flag=1, conta=1;
void atende()                   // atende alarme
{
    printf("alarme # %d\n", conta);
    flag=1;
    conta++;
}

int main(int argc, char** argv)
{
    int fd,c, res;
    struct termios oldtio,newtio;
    char buf[255];
    int i, sum = 0, speed = 0;
    
    if ( (argc < 2) || 
           ((strcmp("/dev/ttyS10", argv[1])!=0) && 
            (strcmp("/dev/ttyS11", argv[1])!=0) )) {
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
    newtio.c_cc[VMIN]     = 0;   /* blocking read until 5 chars received */



  /* 
    VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a 
    leitura do(s) pr髕imo(s) caracter(es)
  */



    tcflush(fd, TCIOFLUSH);

    if ( tcsetattr(fd,TCSANOW,&newtio) == -1) {
      perror("tcsetattr");
      exit(-1);
    }

    printf("New termios structure set\n");

    
    
    char set [5];
    set[0] = FLAG;
    set[1] = A;
    set[2] = SET_C;
    set[3] = A^SET_C;
    set[4] = FLAG;

    volatile int STOP=FALSE;

    int FLAG_RCV=FALSE;
    int A_RCV=FALSE;
    int C_RCV=FALSE;
    int BCC_OK=FALSE;
      
    (void) signal(SIGALRM, atende);  // instala  rotina que atende interrupcao
    while(STOP==FALSE && conta < 4){
      if(flag){
        for (int i=0;i<=4;i++){
          write(fd,&set[i],1);
        }    
        alarm(3);  
        flag=0;
      }
      res = read(fd,buf,1);
      
      switch (buf[0]) {
        case FLAG:
          FLAG_RCV=TRUE;
          if(A_RCV&&C_RCV&&BCC_OK&&FLAG_RCV){
            STOP=TRUE;
          }
          printf("%x\n",buf[0]);
          break;
        case A: 
          if(FLAG_RCV&&!A_RCV)
            A_RCV=TRUE;
          printf("%x\n",buf[0]);
          break;
        case UA_C:
          if(FLAG_RCV&&A_RCV)
              C_RCV=TRUE;
          break;
        case (A^UA_C):
          if(FLAG_RCV&&A_RCV&&C_RCV){
            BCC_OK=TRUE;
            printf("%x\n",buf[0]);
          }

          break;
        default:
          break;
      }
    }
    
    if (conta == 4) printf("correu mal");

    sleep(1);
    if ( tcsetattr(fd,TCSANOW,&oldtio) == -1) {
      perror("tcsetattr");
      exit(-1);
    }
    close(fd);
   
    return 0;
}