#include "protocol.h"

void alarmHandler() // atende alarme
{
    printf("Sending again. Try: # %d\n", try);
    flag = 1;
    try++;
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
        else if (controlField == (UA || RR0 || RR1 || REJ0 || REJ1))
            frame[1] = 0x03;
    }
    frame[2] = controlField;
    frame[3] = frame[1] ^ frame[2];
    frame[4] = FLAG;
}

int sendSupervisionFrame(int fd, int user, unsigned char controlField, unsigned char responseControlField)
{

    unsigned char frameToSend[SUPERVISION_FRAME_SIZE];
    unsigned char responseFrame[SUPERVISION_FRAME_SIZE];

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

    unsigned char responseBuffer[5];

    int FLAG_RCV = FALSE;
    int A_RCV = FALSE;
    int C_RCV = FALSE;
    int BCC_OK = FALSE;
    int STOP = FALSE;

    while (try <= NUMBER_ATTEMPTS)
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
        }

        if (responseControlField == NONE)
        {
            alarm(0);
            return 0;
        }
        else
        {
            if (read(fd, responseBuffer, 1) < 0)
            {
                perror("Error reading supervision frame");
                return -1;
            };

            if (responseBuffer[0] == responseFrame[0])
            {
                FLAG_RCV = TRUE;
                if (A_RCV && C_RCV && BCC_OK && FLAG_RCV)
                {
                    alarm(0);
                    return 0;
                }
            }
            else if (responseBuffer[0] == responseFrame[1])
            {
                if (FLAG_RCV && !A_RCV)
                    A_RCV = TRUE;
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
                }
            }
        }
    }
    if (try == NUMBER_ATTEMPTS + 1)
    {
        return -1;
    }
    return 0;
}

int receiveSupervisionFrame(int fd, unsigned char expectedControlField, unsigned char responseControlField)
{

    unsigned char frameToReceive[SUPERVISION_FRAME_SIZE];
    unsigned char frameToSend[SUPERVISION_FRAME_SIZE];

    createSuperVisionFrame(TRANSMITTER, expectedControlField, frameToReceive);

    createSuperVisionFrame(RECEIVER, responseControlField, frameToSend);

    int FLAG_RCV = FALSE;
    int A_RCV = FALSE;
    int C_RCV = FALSE;
    int BCC_OK = FALSE;
    int STOP = FALSE;

    unsigned char responseBuffer[5];
    while (!STOP)
    {
        if (read(fd, responseBuffer, 1) < 0)
            printf("Error when reading\n");
        printf("Leu %x\n", responseBuffer[0]);
        if (responseBuffer[0] == frameToReceive[0])
        {
            FLAG_RCV = TRUE;
            if (A_RCV && C_RCV && BCC_OK && FLAG_RCV)
            {
                STOP = TRUE;
            }
        }
        else if (responseBuffer[0] == frameToReceive[1])
        {
            if (FLAG_RCV && !A_RCV)
            {
                A_RCV = TRUE;
            }
            else if (FLAG_RCV && A_RCV)
            {
                C_RCV = TRUE;
            }
        }
        else if (responseBuffer[0] == frameToReceive[3])
        {
            if (FLAG_RCV && A_RCV && C_RCV)
            {
                BCC_OK = TRUE;
            }
        }
    }
    for (int i = 0; i < SUPERVISION_FRAME_SIZE; i++)
    {
        write(fd, &(frameToSend[i]), 1);
    }
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

    if (user == TRANSMITTER)
    {

        l1.newtio.c_cc[VTIME] = 0; /* inter-unsigned character timer unused */
        l1.newtio.c_cc[VMIN] = 0;  /* blocking read until 5 unsigned chars received */

        tcflush(fd, TCIOFLUSH);

        if (tcsetattr(fd, TCSANOW, &l1.newtio) == -1)
        {
            perror("tcsetattr");
            return -1;
        }

        (void)signal(SIGALRM, alarmHandler); //Alarm setup

        if (sendSupervisionFrame(fd, TRANSMITTER, SET, UA) < 0)
        { //Sends the frame to the receiver
            printf("No response received. Gave up after %d tries\n", NUMBER_ATTEMPTS);
            return -1;
        }
    }
    else if (user == RECEIVER)
    {
        l1.newtio.c_cc[VTIME] = 0; /* inter-unsigned character timer unused */
        l1.newtio.c_cc[VMIN] = 1;  /* blocking read until 5 unsigned chars received */

        tcflush(fd, TCIOFLUSH);

        if (tcsetattr(fd, TCSANOW, &l1.newtio) == -1)
        {
            perror("tcsetattr");
            return -1;
        }
        receiveSupervisionFrame(fd, SET, UA);
    }

    return fd;
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
    int buffedDataSize = dataSize;
    unsigned char beforeStuffingFrame[dataSize];

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

    int buffedDataSize = byteStuffing(dataSize + 1); //bcc also needs stuffing

    l1.frame[DATA_START + buffedDataSize] = FLAG;

    return DATA_START + buffedDataSize + 1;
}

int byteDestuffing(int buffedDataSize)
{
    unsigned char beforeDestuffingFrame[buffedDataSize];

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
    return indexAfterDestuffing + 1;       //number of bytes after destuffing
}

enum state supervisionEventHandler(unsigned char byteRead, enum state st, unsigned char *supervisionFrame)
{
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
        if (byteRead == FLAG)
            break;
        else if (byteRead == 0x03)
        { // campo de endereço sempre 0x03 nas tramas de informação
            st = A_RCV;
            supervisionFrame[1] = byteRead;
        }
        else
            st = START;
        break;

    case A_RCV:
        if (byteRead == FLAG)
        {
            st = FLAG_RCV;
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
            st = START;
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

//quando emissor lê a supervision frame enviada pelo recetor como resposta a I
unsigned char readSupervisionFrame(int fd)
{
    unsigned char byteRead;
    enum state st = START;
    unsigned char supervisionFrame[SUPERVISION_FRAME_SIZE];

    while (st != STOP)
    {
        read(fd, &byteRead, 1);
        st = supervisionEventHandler(byteRead, st, supervisionFrame);
    }

    return supervisionFrame[2]; // retorna o control field
}

int llwrite(int fd, unsigned char *buffer, int length)
{

    int stuffedFrameSize = createInformationFrame(buffer, length); // length -> tamanho do data

    int sentInformationFrame = FALSE;
    int confirmationReceived = FALSE;
    unsigned char controlField;
    flag = 1;
    try = 1; //restauring values

    (void)signal(SIGALRM, alarmHandler); //Alarm setup

    while (!sentInformationFrame)
    {
        for (int i = 0; i < stuffedFrameSize; i++)
        { //send information frame byte per byte
            write(fd, &l1.frame[i], 1);
        }

        (void)signal(SIGALRM, alarmHandler);
        while (!confirmationReceived && try < NUMBER_ATTEMPTS)
        { //espera pela supervision enviada pelo recetor
            if (flag)
            {
                for (int i = 0; i < stuffedFrameSize; i++)
                { //resending information frame byte per byte
                    write(fd, &l1.frame[i], 1);
                }
                alarm(3);
                flag = 0;
            }
            controlField = readSupervisionFrame(fd);

            if ((controlField == RR0) || (controlField == RR1) || (controlField == REJ0) || (controlField == REJ1))
            {             //if control field != 0, supervision frame was successfull read
                alarm(0); // cancela alarme
                confirmationReceived = TRUE;
            }
        }

        if ((controlField == RR0) || (controlField == RR1))
            sentInformationFrame = TRUE;
        else if ((controlField == REJ0) || (controlField == REJ1))
            sentInformationFrame = FALSE;
        else
            return -1;
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

int saveFrameInBuffer(unsigned char *buffer, int numBytesAfterDestuffing)
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
    char supervisionFrame[5];
    supervisionFrame[0] = FLAG;
    supervisionFrame[1] = 0x03; //valor fixo pq é resposta enviada pelo recetor
    supervisionFrame[2] = responseField;
    supervisionFrame[3] = supervisionFrame[1] ^ supervisionFrame[2];
    supervisionFrame[4] = FLAG;

    for (int i = 0; i < SUPERVISION_FRAME_SIZE; i++)
    {
        write(fd, &supervisionFrame[i], 1);
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
                responseField = nextFrame(sequenceNumber); //ingoring trama
            }
            else if (checkDuplicatedFrame(sequenceNumber) == FALSE)
            {
                saveFrameInBuffer(buffer, numBytesAfterDestuffing);
                doneReadingFrame = 1;
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
                responseField = nextFrame(sequenceNumber); //ingoring trama
            }
            else if (checkDuplicatedFrame(sequenceNumber) == FALSE)
            {
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
        printf("problem sending supervision frame \n");

    return (numBytesRead - 6); // F A C BCC1 DATA BCC2 F => information frame
}
