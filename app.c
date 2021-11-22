#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>

#include "macros.h"

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
    unsigned char *name;
    int size;
    unsigned char *contents;
} file;

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

int createControlPackage(int controlField, unsigned char *package)
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

int createDataPackage(int sequenceNumber, unsigned char *package, unsigned char *data, int dataSize)
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

int readPackageData(int sequenceNumber, unsigned char *package, unsigned char *data)
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
        data = (unsigned char *)malloc(dataSize);
        for (int i = 0; i < dataSize; i++)
        {
            data[i] = package[4 + i];
        }
    }
    else
        return -1;

    return package[0];
}

int receiveFile()
{
    FILE *fp;

    int fd = llopen(RECEIVER_PORT, RECEIVER);
    unsigned char buffer[PACKAGE_SIZE];
    unsigned char *packageData;
    int sizeRead = 0;
    int sequenceNumber = 0;
    int packageType = -1;

    int stop = FALSE;
    while (!stop)
    {

        sizeRead = llread(fd, buffer);

        if (sizeRead < 0)
            printf("Error reading data package");

        packageType = readPackageData(sequenceNumber, buffer, packageData);
        switch (packageType)
        {
        case START:
            fp = fopen(f.name, "w");
            if (fp == NULL)
            {
                perror("Error opening file:",f.name);
                return (-1);
            }
            break;
        case DATA:
            fwrite(packageData, 1, sizeof(packageData), fp);
            sequenceNumber = (sequenceNumber + 1) % 255;
            break;
        case END:
            stop = TRUE;
            break;
        default:
            break;
        }
    }

    free(packageData);
}

int sendFile(unsigned char *fileToSend)
{
    FILE *fp = fopen(fileToSend, "r");
    if (fp == NULL)
    {
        perror("Error in opening file");
        return (-1);
    }

    f.name = fileToSend;

    fseek(fp, 0, SEEK_END);
    f.size = (int)ftell(fp);
    rewind(fp);

    int fd = llopen(TRANSMITTER_PORT, TRANSMITTER);

    unsigned char package[PACKAGE_SIZE];

    int packageSize = createControlPackage(START, package);

    llwrite(fd, package, packageSize);

    int sequenceNumber = 0;

    while (1)
    {
        int sizeRead = fread(package, 1, PACKAGE_SIZE, fp);
        if (sizeRead != PACKAGE_SIZE)
        {
            if (feof(fp))
            {
                packageSize = createDataPackage(sequenceNumber, package, package, sizeRead);

                llwrite(fd, package, packageSize);

                break;
            }
            else
                return -1;
        }

        packageSize = createDataPackage(sequenceNumber, package, package, sizeRead);
        llwrite(fd, package, packageSize);
        sequenceNumber = (sequenceNumber + 1) % 255;
    }

    packageSize = createControlPackage(END, package);

    llwrite(fd, package, packageSize);

    fclose(fp);

    //llclose();
}

int main(int argc, char const *argv[])
{
    sendFile("pinguin.gif");
    return 0;
}
