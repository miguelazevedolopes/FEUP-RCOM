#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)       \
    (byte & 0x80 ? '1' : '0'),     \
        (byte & 0x40 ? '1' : '0'), \
        (byte & 0x20 ? '1' : '0'), \
        (byte & 0x10 ? '1' : '0'), \
        (byte & 0x08 ? '1' : '0'), \
        (byte & 0x04 ? '1' : '0'), \
        (byte & 0x02 ? '1' : '0'), \
        (byte & 0x01 ? '1' : '0')

#define PACKAGE_SIZE 1000

enum CPType
{
    SIZE = 0,
    NAME
};

enum ControlField
{
    DATA = 1,
    START,
    END
};

typedef struct
{
    char *name;
    int size;
    char *contents;
} file;

// typedef struct
// {
//     int fileSize;
//     char *fileName;
// } controlPackage;

file f;

int power(int base, int exponent)
{
    int result = base;
    for (int i = 1; i < exponent; i++)
    {
        result = result * base;
    }
    return result;
}

int createControlPackage(int controlField, char *package)
{
    package[0] = controlField;
    package[1] = SIZE;
    int size = 1;
    for (int i = 1; i < 8; i++)
    {
        size = i;
        if (power(2, i * 8) >= f.size)
        {
            package[2] = i;
            break;
        }
    }

    int aux = f.size;
    for (int i = 0; i < size; i++)
    {
        if (aux >= 256)
        {
            package[3 + i] = aux / 256;
            if ((aux / 256) >= 256)
                aux = aux / 256;
            else
                aux = aux % 256;
        }
        else
        {
            aux = aux % 256;
            package[3 + i] = aux % 256;
        }
    }

    package[3 + size] = NAME;
    package[4 + size] = strlen(f.name);

    for (int i = 0; i < strlen(f.name) + 2; i++)
        package[5 + size + i] = f.name[i];
    return 5 + size + strlen(f.name);
}

int createDataPackage(int sequenceNumber, char *package, char *data, int dataSize)
{
    package[0] = DATA;
    package[1] = sequenceNumber;
    if (dataSize > 256)
    {
        package[2] = dataSize / 256;
        package[3] = dataSize % 256;
    }
    else
    {
        package[2] = 0;
        package[3] = dataSize;
    }
    for (int i = 0; i < dataSize; i++)
    {
        package[4 + i] = data[i];
    }
    return dataSize + 4;
}

int readPackageData(int sequenceNumber, char *package, char *data)
{
    int dataSize = 0;
    int sizeOfDataSize = 0;
    if (package[0] == (START || END))
    {
        if (package[1] != SIZE)
            return -1;
        sizeOfDataSize = package[2];
        for (int i = 0; i < sizeOfDataSize; i++)
        {
            dataSize += power(256, sizeOfDataSize - 1 - i) * package[3 + i];
        }
        f.size = dataSize;
        if (package[3 + sizeOfDataSize] != NAME)
            return -1;
        dataSize = package[4 + sizeOfDataSize];
        for (int i = 0; i < dataSize; i++)
        {
            f.name[i] = package[5 + sizeOfDataSize + i];
        }
    }
    else if (package[0] == DATA)
    {
        if (package[1] != sequenceNumber)
            return -1;
        dataSize = package[2] * 256 + package[3];
        for (int i = 0; i < dataSize; i++)
        {
            data[i] = package[4 + i];
        }
    }
    else
        return -1;

    return package[0];
}

int receiveFile(char *filePath)
{
    FILE *fp = open(filePath, "w");
    if (fp == NULL)
    {
        perror("Error in opening file");
        return (-1);
    }

    //llopen()
    while(1){

        //llread();

        if(readPackageData()==(START)){

        }
        else if(readPackageData()==DATA){
            fwrite(data,1,sizeof(data),fp);
        }

        
    }
    
}

int sendFile(char *fileToSend)
{
    FILE *fp = open(fileToSend, "r");
    if (fp == NULL)
    {
        perror("Error in opening file");
        return (-1);
    }

    //llopen("/dev/ttyS1",TRANSMITTER);

    char package[PACKAGE_SIZE];

    createControlPackage(START, package);

    //llwrite(package) enviar control package

    int sequenceNumber = 0;

    while (1)
    {
        int successfullyRead = fread(package, 1, PACKAGE_SIZE, fp);
        if (successfullyRead != PACKAGE_SIZE)
        {
            if (feof(fp))
            {
                createDataPackage(sequenceNumber, package, package, successfullyRead);

                //llwrite(package) escreve pacote

                break;
            }
            else
                return -1;
        }

        createDataPackage(sequenceNumber, package, package, successfullyRead);
        sequenceNumber = (sequenceNumber + 1) % 255;
        //llwrite(package) escreve pacote
    }

    createControlPackage(END, package);

    //llwrite(package) enviar control package

    fclose(fp);

    //llclose();
}

int main(int argc, char const *argv[])
{
    f.name = "pinguin.gif";
    f.size = 40;
    char package[1000];
    int packageSize = createControlPackage(START, package);
    printf("C: %d\n", package[0]);
    printf("T: %d\n", package[1]);
    printf("L: %d\n", package[2]);
    for (int i = 0; i < package[2]; i++)
    {
        printf(BYTE_TO_BINARY_PATTERN "\n", BYTE_TO_BINARY(package[3 + i]));
    }
    printf("T: %d\n", package[3 + package[2]]);
    printf("L: %d\n", package[4 + package[2]]);
    for (int i = 5 + package[2]; i < packageSize; i++)
    {
        printf("%c", package[i]);
    }
    printf("\n");
    return 0;
}
