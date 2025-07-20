#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>


int main(int argc, char *argv[])
{
    int ret = 0, fd = 0;
    unsigned char buf[64] = {
		0x80, 0x98, 0x67, 0x99
	};	
	unsigned char rx[64];

    fd = open(argv[1], O_RDWR);
    if (fd < 0) {
        printf("Can't open fail %s!\r\n", argv[1]);
        return -1;
    }

    //ite_buf[0] = atoi(argv[2]);

    ret = write(fd, buf, 4);
    if (ret < 0) {
        printf("LED Control failed!\r\n");
        return -1;
    }
    close(fd);

    return 0;
}
