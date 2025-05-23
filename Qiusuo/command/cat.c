#include "syscall.h"
#include "stdio.h"
#include "string.h"
#include "fs.h"	// 这里不该+fs.h的, 不能给用户进程提供fs里面的函数的接口,但没用的话就无所谓了

int main(int argc, char** argv)
{
	if (argc > 2) {
		printf("cat: only support 1 argument.\neg: cat filename\n");
		exit(-2);
	}
	
	// 如果有|并且cat不是第一个命令的话,fd0会被重定位到管道,这里就会从管道拿数据
	if (argc == 1) {
		char buf[512] = {0};
		read(0, buf, 512);
		printf("%s", buf);
		exit(0);
	}

	int buf_size = 1024;
	char abs_path[512] = {0};
	void* buf = malloc(buf_size);
	if (buf == NULL) {
		return -1;
	}
	if (argv[1][0] != '/') {
		getcwd(abs_path, 512);
		strcat(abs_path, "/");
		strcat(abs_path, argv[1]);
	} else {
		strcpy(abs_path, argv[1]);
	}

	int fd = open(abs_path, O_RDONLY);
	if (fd == -1) {
		printf("cat: open: open %s failed\n", argv[1]);
		return -1;
	}

	int read_bytes = 0;
	while (1) {
		read_bytes = read(fd, buf, buf_size);
		if (read_bytes == -1) {
			break;
		}
		write(1, buf, read_bytes);
	}
	free(buf);
	close(fd);
	return 66;
}