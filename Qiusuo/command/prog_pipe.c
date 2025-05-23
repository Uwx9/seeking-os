#include "stdio.h"
#include "syscall.h"
#include "string.h"

int main(int argc, char** argv)
{
	// fork时看父进程一个文件打开了两次, 子进程也应打开两次, fork时会更新打开文件次数
	int fd[2];
	pipe(fd);
	int pid = fork();
	if (pid) {	// 父进程	
		close(fd[0]);	// 关闭输入, 开始时fd_pos = 2
		write(fd[1], "Hi, my son, I love you!", 24);
		return 8;
	} else {
		close(fd[1]);	// 关闭输出
		char buf[32] = {0};
		read(fd[0], buf, 24);
		printf("\nI`m child, my pid is %d\n", getpid()); 
	    printf("I`m child, my father said to me: \"%s\"\n", buf);
		return 9;
	}
}