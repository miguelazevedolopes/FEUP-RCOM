#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

int main(int argc, char const *argv[])
{
    int fd;

    fd = open("/dev/ttyS10", O_RDWR | O_NOCTTY);
    unsigned char c = 0x7e;
    write(fd, &c, 1);
    return 0;
}
