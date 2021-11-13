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

volatile int STOP=FALSE;

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


    
    
    unsigned char F = 0x7E; //flag
    unsigned char A = 0x03; // campo de endereço no emissor
    unsigned char C = 0x03; // SET UP- campo de controlo
    unsigned char BCC =  A^C;
    
    unsigned char set [5];
    set[0] = F;
    set[1] = A;
    set[2] = C;
    set[3] = BCC;
    set[4] = F;
    
    unsigned char miguel [5];
    (void) signal(SIGALRM, atende);  // instala  rotina que atende interrupcao
    while(conta < 4){
        if(flag){

          //res = write(fd,set,5);    

          alarm(3);  
          //res = read(fd,miguel,5);
          
          //printf("adeus");
                         // activa alarme de 3s
          if(miguel[0] == F && miguel[1] == A && miguel[2] == 0x05 && miguel[3] == (A^(0x05)) &&  miguel[4] == F) {
            printf("correu tudo bem");
            break;
          }
           flag=0;
       }
    }
    
    if (conta == 4) printf("correu mal");
    printf("%x\n",miguel[0]);
    printf("%x\n",miguel[1]);
    printf("%x\n",miguel[2]);
    printf("%x\n",miguel[3]);
    printf("%x\n",miguel[4]);
    
    sleep(1);
    if ( tcsetattr(fd,TCSANOW,&oldtio) == -1) {
      perror("tcsetattr");
      exit(-1);
    }
    close(fd);
   
    return 0;
}