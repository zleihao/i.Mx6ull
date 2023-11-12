#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    int ret = 0;
    int fd = 0;
    int val;

    if (argc < 3) {
        printf("用法错误\n");
        return -1;
    }

    fd = open(argv[1], O_RDWR);
    if (fd < 0) {
        printf("Open file fail: %s\n", argv[1]);
        return -2;
    }

    val = atoi(argv[2]);

    ret = write(fd, &val, 1);
    if (ret < 0) {
        printf("write data err: %d\r\n", *argv[2]);
    }

    close(fd);
}