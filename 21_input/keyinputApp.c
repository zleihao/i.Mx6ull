#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/input.h>

static struct input_event inputevent;

int main(int argc, char *argv[])
{
	int fd;
	int ret = 0;
	char *filename;
	
	if (argc != 2) {
		printf("Error Usage!\r\n");
		return -1;
	}

	filename = argv[1];
	fd = open(filename, O_RDWR);
	if (fd < 0) {
		printf("Can't open file %s\r\n", filename);
		return -1;
	}

	while (1) {
		ret = read(fd, &inputevent, sizeof(inputevent));
		if (ret > 0) {
			switch (inputevent.type) {
				case EV_KEY:
					if (inputevent.code < BTN_MISC) {
						printf("key %d %s\r\n", inputevent.code, inputevent.value ? "按下" : "抬起");
					} else {
						printf("button %d %s\r\n", inputevent.code, inputevent.value ? "按下" : "抬起");
					}
					break;
				case EV_REP:
					break;
				case EV_SYN:
					break;
			
			}
		} else {
			printf("Read err\r\n");
		}
	}

	close(fd);
	return ret;
}