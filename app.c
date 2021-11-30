#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>

#include "macros.h"
#include "protocol.c"

enum CPType
{
    SIZE = 0,
    NAME
};

enum ControlField
{
    CF_DATA = 1,
    CF_START,
    CF_END
};

typedef struct
{
    unsigned char *name;
    int size;
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
    package[0] = CF_DATA;
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
    if (package[0] == CF_START)
    {
        if (package[1] != SIZE)
        {
            printf("Wrong type field, should be SIZE\n");
            return -1;
        }

        sizeOfDataSize = package[2];
        for (int i = 0; i < sizeOfDataSize; i++)
        {
            dataSize += power(256, sizeOfDataSize - 1 - i) * package[3 + i];
        }
        f.size = dataSize;
        if (package[3 + sizeOfDataSize] != NAME)
        {
            printf("Wrong type field, should be NAME\n");
            return -1;
        }
        dataSize = package[4 + sizeOfDataSize];
        f.name = (unsigned char *)malloc(dataSize);
        for (int i = 0; i < dataSize; i++)
        {
            f.name[i] = package[5 + sizeOfDataSize + i];
        }
    }
    else if (package[0] == CF_END)
    {
        if (package[1] != SIZE)
        {
            printf("Wrong type field, should be SIZE\n");
            return -1;
        }
        sizeOfDataSize = package[2];
        for (int i = 0; i < sizeOfDataSize; i++)
        {
            dataSize += power(256, sizeOfDataSize - 1 - i) * package[3 + i];
        }
        if (f.size != dataSize)
        {
            printf("END package doesn't match with START package\n");
            return -1;
        }

        if (package[3 + sizeOfDataSize] != NAME)
        {
            printf("Wrong type field, should be NAME\n");
            return -1;
        }
        dataSize = package[4 + sizeOfDataSize];
        for (int i = 0; i < dataSize; i++)
        {
            if (f.name[i] != package[5 + sizeOfDataSize + i])
            {
                printf("END package doesn't match with START package\n");
                return -1;
            }
        }
    }
    else if (package[0] == CF_DATA)
    {
        if (package[1] != sequenceNumber)
        {
            printf("Wrong sequence number\n");
            return -1;
        }
        dataSize = package[2] * 256 + package[3];
        for (int i = 0; i < dataSize; i++)
        {
            data[i] = package[4 + i];
        }
    }
    else
    {
        printf("Not a valid package\n");
        return -1;
    }

    return package[0];
}

int receiveFile()
{
    FILE *fp;

    int fd = llopen(RECEIVER_PORT, RECEIVER);

    if (fd < 0)
    {
        printf("Error opening protocol on the receiver side.\n");
        return -1;
    }
    else
        printf("Receiver protocol open!\n");
    unsigned char buffer[PACKAGE_SIZE]; //buffer size is allocated inside the llread function
    unsigned char packageData[PACKAGE_DATA_SIZE];
    int sizeRead = 0;
    int sequenceNumber = 0;
    int packageType = -1;

    int stop = FALSE;
    while (!stop)
    {
        sizeRead = llread(fd, buffer);
        if (sizeRead < 0)
        {
            printf("Error reading data package\n");
            return -1;
        }
        packageType = readPackageData(sequenceNumber, buffer, packageData);
        switch (packageType)
        {
        case CF_START:
            fp = fopen(f.name, "w");
            if (fp == NULL)
            {
                perror(f.name);
                return (-1);
            }
            break;
        case CF_DATA:
            fwrite(packageData, 1, sizeRead - 4, fp);
            sequenceNumber = (sequenceNumber + 1) % 255;
            break;
        case CF_END:

            stop = TRUE;
            break;
        default:
            break;
        }
    }
    fclose(fp);
    if (llclose(fd, RECEIVER) != -1)
        printf("Receiver protocol closed!\n");
}

int sendFile(const char *fileToSend)
{
    FILE *fp = fopen(fileToSend, "r");
    if (fp == NULL)
    {
        perror("Error in opening file\n");
        return (-1);
    }

    f.name = (unsigned char *)malloc(sizeof(fileToSend));
    strcpy(f.name, fileToSend);

    fseek(fp, 0, SEEK_END);
    f.size = (int)ftell(fp);
    rewind(fp);

    int fd = llopen(TRANSMITTER_PORT, TRANSMITTER);

    if (fd == -1)
    {
        printf("Error opening protocol on the transmitter side.\n");
        return -1;
    }
    else
        printf("Transmitter protocol open!\n");

    unsigned char package[PACKAGE_SIZE];
    unsigned char data[PACKAGE_DATA_SIZE];

    int packageSize = createControlPackage(CF_START, package);
    if (llwrite(fd, package, packageSize) == -1)
    {
        printf("Error when writing the START control package\n");
        return -1;
    }

    int sequenceNumber = 0;

    while (1)
    {
        int sizeRead = fread(data, 1, PACKAGE_DATA_SIZE, fp);
        packageSize = createDataPackage(sequenceNumber, package, data, sizeRead);
        if (sizeRead != PACKAGE_DATA_SIZE)
        {
            if (feof(fp))
            {

                if (llwrite(fd, package, packageSize) == -1)
                {
                    printf("Error when writing the last data package\n");
                    return -1;
                }
                break;
            }
            else
                return -1;
        }

        if (llwrite(fd, package, packageSize) == -1)
        {
            printf("Error when writing the DATA control package\n");
            return -1;
        }
        sequenceNumber = (sequenceNumber + 1) % 255;
    }

    packageSize = createControlPackage(CF_END, package);

    if (llwrite(fd, package, packageSize) == -1)
    {
        printf("Error when writing the END control package\n");
        return -1;
    }

    fclose(fp);

    printf("Finished sending file\n");
    if (llclose(fd, TRANSMITTER) != -1)
        printf("Transmitter protocol closed!\n");
}

int main(int argc, char const *argv[])
{
    if (strcmp("receiver", argv[1]) == 0 && argc == 2)
    {
        receiveFile();
    }
    else if (strcmp("transmitter", argv[1]) == 0 && argc == 3)
    {
        sendFile(argv[2]);
    }
    else
    {
        printf("Usage: %s transmitter filename or %s receiver\n", argv[0], argv[0]);
        return 1;
    }
    return 0;
}
