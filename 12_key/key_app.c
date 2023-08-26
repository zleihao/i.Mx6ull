#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#define KEY_VALUE   0xf0
#define INVA_VALUE  0x00

int main(int argc, char *argv[])
{
    int value = 0, fd = 0;
    int err = 0, cnt = 0;

    if (argc < 2) {
        printf("Error using!\r\n");
        return -1;
    }

    fd = open(argv[1], O_RDWR);
    if (fd < 0) {
        printf("Can't open fail %s!\r\n", argv[1]);
        return -1;
    }

    while (1) {
        err = read(fd, &value, sizeof(value));
        if (value == KEY_VALUE) {
            cnt++;
            printf("User press key, curr num is %d!\r\n", cnt);
        }
    }

    close(fd);

    return 0;
}