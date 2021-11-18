
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

#define DATA_START 3
#define ESCAPE 0x7D

#define CONTROL_FIELD_O  0x00 //00000000
#define CONTROL_FIELD_1  0x40; //01000000

//RESPOSTAS AO WRITE
#define RR0 0x05  //00000101
#define RR1 0x85  //10000101
#define REJ0 0x01 //00000001
#define REJ1 0x81 //10000001
