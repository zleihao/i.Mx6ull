#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>

int main(int argc, char *argv[])
{
	int fd;
	char *filename;
	unsigned char databuf[2];
    unsigned int data;
	int ret = 0;

	if (argc != 2) {
		printf("Error Usage!\r\n");
		return -1;
	}

	filename = argv[1];
	fd = open(filename, O_RDWR);
	if(fd < 0) {
		printf("can't open file %s\r\n", filename);
		return -1;
	}

	while (1) {
		ret = read(fd, databuf, sizeof(databuf));
		if(ret == 0) { 			/* 数据读取成功 */
			data = (databuf[0] << 8) | (databuf[1]);
			printf("ps = %lf\r\n", data / 1.2);
		}
		usleep(200000); /*100ms */
	}
	close(fd);	/* 关闭文件 */	
}