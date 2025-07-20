#include "stdio.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "sys/ioctl.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"
#include <poll.h>
#include <sys/select.h>
#include <sys/time.h>
#include <signal.h>
#include <fcntl.h>

/*
 * @description		: main主程序
 * @param - argc 	: argv数组元素个数
 * @param - argv 	: 具体参数
 * @return 			: 0 成功;其他 失败
 */
int main(int argc, char *argv[])
{
	int fd;
	char *filename;
	int ret = 0;
	float humidity, temperature;
	unsigned char buf[5];

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
		ret = read(fd, buf, 5);
		if (ret < 0) {
			printf("ath10_get_temp_humi fail!\n");
		} else {
			unsigned int humi, temp;
			// 转换数据
			humi = (buf[0] << 12) | (buf[1] << 4) | (buf[2] >> 4);
			temp = ((buf[2] & 0X0F) << 16) | (buf[3] << 8) | (buf[4]);

			humidity = (humi * 100.0 / 1024 / 1024 + 0.5);
			temperature = (temp * 2000.0 / 1024 / 1024 + 0.5) / 10.0 - 50;

			printf("temperature = %.04f°C, humidity = %.04f°C\n", temperature, humidity);
		}

		sleep(1);
	}

	return 0;
}

