#include "protocol.h"

void alarmHandler() // atende alarme
{
    printf("Sending again. Try: # %d\n", try);
    flag = 1;
    try++;
}

void configureAlarm()
{
    struct sigaction action;
    action.sa_handler = alarmHandler;

    if (sigemptyset(&action.sa_mask) == -1)
    {
        perror("sigemptyset");
        exit(-1);
    }

    action.sa_flags = 0; //

    if (sigaction(SIGALRM, &action, NULL) != 0)
    {
        perror("sigaction");
        exit(-1);
    }
    flag = 1;
    try = 1; //restarting values
}

int createSuperVisionFrame(int user, unsigned char controlField, unsigned char *frame)
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
        else if ((controlField == UA) || (controlField == RR0) || (controlField == RR1) || (controlField == REJ0) || (controlField == REJ1))
            frame[1] = 0x03;
    }
    frame[2] = controlField;
    frame[3] = frame[1] ^ frame[2];
    frame[4] = FLAG;
}

int sendSupervisionFrame(int fd, int user, unsigned char controlField)
{
    unsigned char frameToSend[SUPERVISION_FRAME_SIZE];

    if (user == TRANSMITTER)
    {
        createSuperVisionFrame(TRANSMITTER, controlField, frameToSend); //Creates the frame to send
    }
    else if (user == RECEIVER)
    {
        createSuperVisionFrame(RECEIVER, controlField, frameToSend); //Creates the frame to send
    }
    enum state st = START;
    unsigned char responseByte;
    configureAlarm();

    while (try <= NUMBER_ATTEMPTS && st != STOP)
    {

        if (flag)
        {
            for (int i = 0; i < SUPERVISION_FRAME_SIZE; i++)
            {
                if (write(fd, &(frameToSend[i]), 1) < 0)
                {
                    perror("Error writing supervision frame");
                    return -1;
                };
            }
            alarm(ALARM_WAIT_TIME);
            flag = 0;
            printf("Escreveu. State: %d\n", st);
            st = START;
        }
        if (read(fd, &responseByte, 1) < 0)
            printf("Error when reading\n");
        st = supervisionEventHandler(responseByte, st, l1.frame);
    }
    alarm(0);
    if (try == NUMBER_ATTEMPTS + 1)
    {
        return -1;
    }
    return 0;
}

int receiveSupervisionFrame(int fd, unsigned char expectedControlField, unsigned char responseControlField)
{

    enum state st = START;
    unsigned char responseBuffer;
    while (st != STOP)
    {
        printf("Entrou\n");

        read(fd, &responseBuffer, 1);

        printf("Nao está preso no read\n");
        st = supervisionEventHandler(responseBuffer, st, l1.frame);
        if (st == STOP && expectedControlField != l1.frame[2])
        {
            printf("Didn't expect this frame. Got %x but expected %x", l1.frame[2], expectedControlField);
        }
        printf("Leu. State: %d\n", st);
    }

    if (responseControlField == NONE)
        return 0;

    unsigned char frameToSend[SUPERVISION_FRAME_SIZE];

    createSuperVisionFrame(RECEIVER, responseControlField, frameToSend);

    for (int i = 0; i < SUPERVISION_FRAME_SIZE; i++)
    {
        write(fd, &(frameToSend[i]), 1);
    }
    printf("Escreveu.\n");
    return 0;
}

int llopen(unsigned char *port, int user)
{
    int fd;

    fd = open(port, O_RDWR | O_NOCTTY);
    if (fd < 0)
    {
        perror(port);
        return -1;
    }
    if (tcgetattr(fd, &l1.oldtio) == -1)
    { /* save current port settings */
        perror("tcgetattr");
        return -1;
    }

    bzero(&l1.newtio, sizeof(l1.newtio));
    l1.newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    l1.newtio.c_iflag = IGNPAR;
    l1.newtio.c_oflag = 0;

    /* set input mode (non-canonical, no echo,...) */
    l1.newtio.c_lflag = 0;

    l1.newtio.c_cc[VTIME] = 0; /* inter-unsigned character timer unused */
    l1.newtio.c_cc[VMIN] = 1;  /* blocking read until 1 unsigned chars received */

    tcflush(fd, TCIOFLUSH);

    if (tcsetattr(fd, TCSANOW, &l1.newtio) == -1)
    {
        perror("tcsetattr");
        return -1;
    }

    if (user == TRANSMITTER)
    {

        if (sendSupervisionFrame(fd, TRANSMITTER, SET) < 0)
        { //Sends the frame to the receiver
            printf("No response received. Gave up after %d tries\n", NUMBER_ATTEMPTS);
            return -1;
        }
    }
    else if (user == RECEIVER)
    {

        receiveSupervisionFrame(fd, SET, UA);
    }

    return fd;
}

int llclose(int fd, int user)
{
    if (user == TRANSMITTER)
    {
        printf("Entrou\n");
        if (sendSupervisionFrame(fd, TRANSMITTER, DISC) < 0)
        { //Sends the frame to the receiver
            printf("No response received. Gave up after %d tries", NUMBER_ATTEMPTS);
            exit(1);
        }
        printf("Enviou\n");
        receiveSupervisionFrame(fd, DISC, UA);
        printf("Enviou2\n");
    }
    else if (user == RECEIVER)
    {
        printf("Entrou\n");
        receiveSupervisionFrame(fd, DISC, NONE);
        printf("Enviou\n");
        sendSupervisionFrame(fd, RECEIVER, DISC);
        printf("Enviou2\n");
    }
    close(fd);
}

unsigned char createBCC2(int dataSize)
{

    unsigned char bcc2 = l1.frame[DATA_START];

    for (int i = 0; i < dataSize; i++)
    {
        bcc2 = bcc2 ^ l1.frame[DATA_START + i];
    }

    return bcc2;
}

int byteStuffing(int dataSize)
{
    int stuffedDataSize = dataSize;
    unsigned char beforeStuffingFrame[dataSize];
    int indexBeforeStuffing = 0;
    for (int i = DATA_START; i < (DATA_START + dataSize); i++)
    {
        beforeStuffingFrame[indexBeforeStuffing] = l1.frame[i];
        indexBeforeStuffing++;
    }

    int counter = 0;
    int indexAfterStuffing = DATA_START;
    indexBeforeStuffing = 0;
    while (counter < (dataSize * 2))
    {
        if (beforeStuffingFrame[indexBeforeStuffing] == FLAG)
        {
            l1.frame[indexAfterStuffing] = ESCAPE;
            l1.frame[indexAfterStuffing + 1] = 0x5E;
            stuffedDataSize += 1;
            indexAfterStuffing += 2;
        }
        else if (beforeStuffingFrame[indexBeforeStuffing] == ESCAPE)
        {
            l1.frame[indexAfterStuffing] = ESCAPE;
            l1.frame[indexAfterStuffing + 1] = 0x5D;
            stuffedDataSize += 1;
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

    return stuffedDataSize;
}

int createInformationFrame(unsigned char *data, int dataSize)
{
    l1.frame[0] = FLAG;
    l1.frame[1] = 0x03; //valor fixo pq só emissor envia I e I é um comando

    //control field
    if (l1.sequenceNumber == 0)
        l1.frame[2] = CONTROL_FIELD_0;
    else
        l1.frame[2] = CONTROL_FIELD_1;

    l1.frame[3] = l1.frame[1] ^ l1.frame[2];

    for (int i = 0; i < dataSize; i++)
    {
        l1.frame[DATA_START + i] = data[i];
    }
    l1.frame[DATA_START + dataSize] = createBCC2(dataSize);

    int stuffedDataSize = byteStuffing(dataSize + 1); //bcc also needs stuffing

    l1.frame[DATA_START + stuffedDataSize] = FLAG;

    return DATA_START + stuffedDataSize + 1;
}

int byteDestuffing(int stuffedDataSize)
{
    unsigned char beforeDestuffingFrame[DATA_START + stuffedDataSize];

    for (int i = DATA_START; i < (DATA_START + stuffedDataSize); i++)
    {
        beforeDestuffingFrame[i] = l1.frame[i];
    }

    int indexAfterDestuffing = DATA_START;
    int indexBeforeDeStuffing = DATA_START;

    while (indexBeforeDeStuffing < (stuffedDataSize - 1))
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
    return indexAfterDestuffing + 1;       //number of bytes after destuffing
}

enum state supervisionEventHandler(unsigned char byteRead, enum state st, unsigned char *supervisionFrame)
{
    //printf("Byte: %x\n", byteRead);
    switch (st)
    {
    case START:
        if (byteRead == FLAG)
        {
            st = FLAG_RCV;
            supervisionFrame[0] = byteRead;
        }
        break;
    case FLAG_RCV:
        if ((byteRead == 0x03) || (byteRead == 0x01))
        {
            st = A_RCV;
            supervisionFrame[1] = byteRead;
        }
        else if (byteRead != FLAG)
        {
            st = START;
        }
        break;

    case A_RCV:
        if (byteRead == FLAG)
        {
            st = FLAG_RCV;
        }
        else if ((byteRead == SET) || (byteRead == UA) || (byteRead == DISC))
        {
            st = C_RCV;
            supervisionFrame[2] = byteRead;
        }
        else if (l1.sequenceNumber == 0)
        {
            if ((byteRead == RR1) || (byteRead == REJ0))
            {
                st = C_RCV;
                supervisionFrame[2] = byteRead;
            }
        }
        else if (l1.sequenceNumber == 1)
        {
            if ((byteRead == RR0) || (byteRead == REJ1))
            {
                st = C_RCV;
                supervisionFrame[2] = byteRead;
            }
        }
        break;

    case C_RCV:
        if (byteRead == (supervisionFrame[1] ^ supervisionFrame[2]))
        { // cálculo do bcc para confirmação
            st = BCC_OK;
            supervisionFrame[3] = byteRead;
        }
        else if (byteRead == FLAG)
            st = FLAG_RCV;
        else
        {
            st = START;
        }
        break;
    case BCC_OK:
        if (byteRead == FLAG)
        {
            st = STOP;
            supervisionFrame[4] = byteRead;
        }
        else
            st = START;
        break;
    default:
        break;
    }
    return st;
}

int llwrite(int fd, unsigned char *buffer, int length)
{

    int stuffedFrameSize = createInformationFrame(buffer, length); // length -> tamanho do data

    unsigned char controlField;

    unsigned char byteRead;
    enum state st = START;

    configureAlarm();
    int stop = FALSE;

    unsigned char supervisionFrame[SUPERVISION_FRAME_SIZE];

    while (try <= NUMBER_ATTEMPTS && !stop)
    { //espera pela supervision enviada pelo recetor
        if (flag)
        {

            for (int i = 0; i < stuffedFrameSize; i++)
            { //resending information frame byte per byte
                if (write(fd, &l1.frame[i], 1) < 0)
                {
                    printf("Error writing information frame\n");
                }
            }
            alarm(ALARM_WAIT_TIME);
            flag = 0;
        }
        read(fd, &byteRead, 1);

        // if (byteRead != FLAG && byteRead != 0)
        //     printf("Byte: %x\n", byteRead);

        st = supervisionEventHandler(byteRead, st, supervisionFrame);
        //printf("State: %d\n", st);
        if (st == STOP)
        {
            controlField = supervisionFrame[2];
            if ((controlField == RR0) || (controlField == RR1))
            {             //if control field != 0, supervision frame was successfull read
                alarm(0); // cancela alarme
                stop = TRUE;
                break;
            }
            else if ((controlField == REJ0) || (controlField == REJ1))
            {
                alarm(0);
                flag = 1;
                try++;
                st = START;
            }
        }
    }
    if (l1.sequenceNumber == 0)
        l1.sequenceNumber = 1;
    else if (l1.sequenceNumber == 1)
        l1.sequenceNumber = 0;
    else
        return -1;
    return stuffedFrameSize; // retorno : número de carateres escritos
}

enum state informationEventHandler(unsigned char byteRead, enum state st, int *buffedFrameSize)
{
    switch (st)
    {
    case START:
        *buffedFrameSize = 0;
        if (byteRead == FLAG)
        {
            st = FLAG_RCV;
            l1.frame[*buffedFrameSize] = byteRead;
            *buffedFrameSize = *buffedFrameSize + 1;
        }
        break;

    case FLAG_RCV:
        if (byteRead == FLAG)
            break;
        else if (byteRead == 0x03)
        { // campo de endereço sempre 0x03 nas tramas de informação
            st = A_RCV;
            l1.frame[*buffedFrameSize] = byteRead;
            *buffedFrameSize = *buffedFrameSize + 1;
        }
        else
            st = START; //buffedFrameSize restored to 0
        break;

    case A_RCV:
        if (byteRead == FLAG)
        {
            st = FLAG_RCV;
            *buffedFrameSize = 1;
        }
        else if (l1.sequenceNumber == 0)
        {
            if (byteRead == CONTROL_FIELD_0)
            {
                st = C_RCV;
                l1.frame[*buffedFrameSize] = byteRead;
                *buffedFrameSize = *buffedFrameSize + 1;
            }
        }
        else if (l1.sequenceNumber == 1)
        {
            if (byteRead == CONTROL_FIELD_1)
            {
                st = C_RCV;
                l1.frame[*buffedFrameSize] = byteRead;
                *buffedFrameSize = *buffedFrameSize + 1;
            }
        }
        else
        {
            st = START;
        }
        break;

    case C_RCV:
        if (byteRead == (l1.frame[1] ^ l1.frame[2]))
        { // cálculo do bcc para confirmação
            st = BCC_OK;
            l1.frame[*buffedFrameSize] = byteRead;
            *buffedFrameSize = *buffedFrameSize + 1;
        }
        else if (byteRead == FLAG)
        {
            st = FLAG_RCV;
            *buffedFrameSize = 1;
        }
        else
            st = START;
        break;

    case BCC_OK:
        if (byteRead == FLAG)
        {
            l1.frame[*buffedFrameSize] = byteRead;
            *buffedFrameSize = *buffedFrameSize + 1;
            st = STOP;
        }
        else
        {
            l1.frame[*buffedFrameSize] = byteRead;
            *buffedFrameSize = *buffedFrameSize + 1; //Data
        }

    default:
        break;
    }
    return st;
}

// quando recetor lê information frame enviada por emissor
int readInformationFrame(int fd)
{
    unsigned char byteRead;
    enum state st = START;
    int buffedFrameSize;
    while (st != STOP)
    {
        read(fd, &byteRead, 1);
        st = informationEventHandler(byteRead, st, &buffedFrameSize);
    }
    return buffedFrameSize;
}

int checkBCC2(int numBytesAfterDestuffing)
{
    int dataSize = numBytesAfterDestuffing - 6;
    if (l1.frame[numBytesAfterDestuffing - 2] == createBCC2(dataSize))
        return TRUE;
    return FALSE;
}

unsigned char nextFrame(int sequenceNumber)
{
    unsigned char responseByte;
    if (sequenceNumber == 0)
    {
        responseByte = RR1;
        l1.sequenceNumber = 1;
    }
    else
    {
        responseByte = RR0;
        l1.sequenceNumber = 0;
    }
    return responseByte;
}

unsigned char resendFrame(int sequenceNumber)
{
    unsigned char responseByte;
    if (sequenceNumber == 0)
    {
        responseByte = REJ0;
        l1.sequenceNumber = 0;
    }
    else
    {
        responseByte = REJ1;
        l1.sequenceNumber = 1;
    }
    return responseByte;
}

int checkDuplicatedFrame(int sequenceNumber)
{
    if (sequenceNumber != l1.sequenceNumber)
        return TRUE;
    else if (sequenceNumber == l1.sequenceNumber)
        return FALSE;
    else
        return -1;
}

int getSequenceNumber()
{
    if (l1.frame[2] == CONTROL_FIELD_0)
        return 0;
    else if (l1.frame[2] == CONTROL_FIELD_1)
        return 1;
    return -1;
}

int saveDataInBuffer(unsigned char *buffer, int numBytesAfterDestuffing)
{ //só passamos data bytes para buffer
    int index = 0;
    for (int i = DATA_START; i < (numBytesAfterDestuffing - 2); i++)
    {
        buffer[index] = l1.frame[i];
        index++;
    }
    return 0;
}

int sendConfirmation(int fd, unsigned char responseField)
{
    unsigned char supervisionFrame[SUPERVISION_FRAME_SIZE];
    supervisionFrame[0] = FLAG;
    supervisionFrame[1] = 0x03; //valor fixo pq é resposta enviada pelo recetor
    supervisionFrame[2] = responseField;
    supervisionFrame[3] = supervisionFrame[1] ^ supervisionFrame[2];
    supervisionFrame[4] = FLAG;

    for (int i = 0; i < SUPERVISION_FRAME_SIZE; i++)
    {
        write(fd, &(supervisionFrame[i]), 1);
    }
    return 0;
}

int llread(int fd, unsigned char *buffer)
{

    int doneReadingFrame = FALSE;
    int numBytesRead;
    int numBytesAfterDestuffing;
    int sequenceNumber;
    unsigned char responseField;
    while (!doneReadingFrame)
    {
        numBytesRead = readInformationFrame(fd);
        numBytesAfterDestuffing = byteDestuffing(numBytesRead);
        if (checkBCC2(numBytesAfterDestuffing) == TRUE)
        { // F A C BCC1 DATA BCC2 F => information frame
            sequenceNumber = getSequenceNumber();
            if (checkDuplicatedFrame(sequenceNumber) == TRUE)
            {
                //printf("Tou aqui 1\n");
                responseField = nextFrame(sequenceNumber); //ingoring trama
            }
            else if (checkDuplicatedFrame(sequenceNumber) == FALSE)
            {
                //printf("Tou aqui 2\n");
                saveDataInBuffer(buffer, numBytesAfterDestuffing);
                doneReadingFrame = TRUE;
                responseField = nextFrame(sequenceNumber); //trama sucessfully read
            }
            else
                return -1;
        }
        else if (checkBCC2(numBytesRead) == FALSE)
        {
            sequenceNumber = getSequenceNumber();
            if (checkDuplicatedFrame(sequenceNumber) == TRUE)
            {
                //printf("Tou aqui 3\n");
                responseField = nextFrame(sequenceNumber); //ingoring trama
            }
            else if (checkDuplicatedFrame(sequenceNumber) == FALSE)
            {
                //printf("Tou aqui 4\n");
                responseField = resendFrame(sequenceNumber); //tinha um erro tem de ser mandada again
            }
            else
                return -1;
        }
        else
            return -1;
    }

    //sends response
    if (sendConfirmation(fd, responseField) != 0)
        printf("Problem sending supervision frame \n");
    printf("Numero bytes: %d\n", numBytesAfterDestuffing - 6);
    return (numBytesAfterDestuffing - 6); // F A C BCC1 DATA BCC2 F => information frame
}
