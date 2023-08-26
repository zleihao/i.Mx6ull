#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CLOSE_CMD         _IO(0xef, 1)        //关闭
#define OPEN_CMD          _IO(0xef, 2)        //打开
#define SET_PERIOD_CMD    _IOW(0xef, 3, int)  //设置周期数


int main(int argc, char *argv[])
{
    int fd = 0;
    int ret = 0, cmd = 0, arg = 0;
    char str[100];

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
        printf("Input CMD:");
		ret = scanf("%d", &cmd);
		if (ret != 1) {				/* 参数输入错误 */
			gets(str);				/* 防止卡死 */
		}

		if(cmd == 1)				/* 关闭LED灯 */
			cmd = CLOSE_CMD;
		else if(cmd == 2)			/* 打开LED灯 */
			cmd = OPEN_CMD;
		else if(cmd == 3) {
			cmd = SET_PERIOD_CMD;	/* 设置周期值 */
			printf("Input Timer Period:");
			ret = scanf("%d", &arg);
			if (ret != 1) {			/* 参数输入错误 */
				gets(str);			/* 防止卡死 */
			}
		}
		ioctl(fd, cmd, arg);		/* 控制定时器的打开和关闭 */	
    }

    close(fd);

    return 0;
}