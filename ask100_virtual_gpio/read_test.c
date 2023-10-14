#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char **argv)
{
        int fd = open(argv[1], O_RDONLY);
    
        char buf[10];
    
        read(fd, buf, 10);
        printf("%s\n", buf);
        return 0;
}