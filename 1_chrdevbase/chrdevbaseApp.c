#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    int ret = 0, fd = 0;
    char read_buf[100];
    char write_buf[100] = "App write data";

    if (argc < 3) {
        printf("Error using!\r\n");
        return -1;
    }

    fd = open(argv[1], O_RDWR);
    if (fd < 0) {
        printf("Can't open fail %s!\r\n", argv[1]);
        return -1;
    }

    if (atoi(argv[2]) == 1) {
        ret = read(fd, read_buf, 50);
        if (ret < 0) {
            printf("read file fail %s\r\n", argv[1]);
            return -2;
        } else {
            printf("read data:%s\r\n", read_buf);
        }
    } 
    if (atoi(argv[2]) == 2) { 
        ret = write(fd, write_buf, 50);
        if (ret < 0) {
            printf("write file fail %s\r\n", argv[1]);
            return -3;
        } else {
            printf("write data:%s\r\n", write);
        }
    }

    close(fd);

    return 0;
}