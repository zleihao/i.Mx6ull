#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#define LED_OFF    0  //关灯
#define LED_ON     1  //开灯

int main(int argc, char *argv[])
{
    int ret = 0, fd = 0;
    char write_buf[1];

    if (argc < 3) {
        printf("Error using!\r\n");
        return -1;
    }

    fd = open(argv[1], O_RDWR);
    if (fd < 0) {
        printf("Can't open fail %s!\r\n", argv[1]);
        return -1;
    }

    write_buf[0] = atoi(argv[2]);

    ret = write(fd, write_buf, 1);
    if (ret < 0) {
        printf("LED Control failed!\r\n");
        return -1;
    }

    close(fd);

    return 0;
}