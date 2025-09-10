#include "print.h"
#include "init.h"
#include "thread.h"
#include "interrupt.h"
#include "memory.h"
#include "list.h"
#include "console.h"
#include "keyboard.h"
#include "ioqueue.h"
#include "process.h"
#include "syscall-init.h"
#include "syscall.h"
#include "../lib/stdio.h"
#include "fs.h"
#include "string.h"
#include "dir.h"
#include "shell.h"
#include "stdio-kernel.h"
#include "assert.h"
#include"wait_exit.h"
#include "inode.h"

void init(void);

int main(void)
{
	put_str("I'm kernel!\n", 0x07);
	init_all();
	
	// //写入应用程序
	// uint32_t file_size = 11912;
	// uint32_t sec_cnt = DIV_ROUND_UP(file_size, SECTOR_SIZE);
	// struct disk* sda = &channels[0].devices[0];

	// // //malloc 的大小应该由sec_cnt决定,因为最后要往buf中写入的数据量是由sec_cnt决定的,直接用file_size可能会有误差
	// uint32_t malloc_size = sec_cnt * 512;
	// void* prog_buf = sys_malloc(malloc_size);
	// ide_read(sda, 500, prog_buf, sec_cnt);
	// int32_t fd = sys_open("/cat", O_CREATE | O_RDWR);
	// if (fd != -1) {
	// 	if (sys_write(fd, prog_buf, file_size) == -1) {
	// 		printk("file write error\n");
	// 		while (1);
	// 	}
	// }
	// sys_free(prog_buf);
	// 写入应用程序ok

	// int32_t fd = sys_open("/file1", O_CREATE | O_RDWR);
	// if (fd != -1) {
	// 	if (sys_write(fd, "hello,world!\nnihao,shijie!\n", 27) == -1) {
	// 		printk("file write error\n");
	// 		while (1);
	// 	}
	// }
	
	// 写入应用程序
	// uint32_t file_size = 12036;
	// uint32_t sec_cnt = DIV_ROUND_UP(file_size, SECTOR_SIZE);
	// struct disk* sda = &channels[0].devices[0];
	
	// // //malloc 的大小应该由sec_cnt决定,因为最后要往buf中写入的数据量是由sec_cnt决定的,直接用file_size可能会有误差
	// uint32_t malloc_size = sec_cnt * 512;
	// void* prog_buf = sys_malloc(malloc_size);
	// ide_read(sda, 500, prog_buf, sec_cnt);
	// int32_t fd = sys_open("/cat", O_CREATE | O_RDWR);
	// if (fd != -1) {
	// 	if (sys_write(fd, prog_buf, file_size) == -1) {
	// 		printk("file write error\n");
	// 		while (1);
	// 	}
	// }
	// sys_free(prog_buf);
	// 写入应用程序ok			

	// 写入应用程序
	uint32_t file_size = 11700;
	uint32_t sec_cnt = DIV_ROUND_UP(file_size, SECTOR_SIZE);
	struct disk* sda = &channels[0].devices[0];
	void* prog_buf = sys_malloc(file_size);
	ide_read(sda, 500, prog_buf, sec_cnt);
	int32_t fd = sys_open("/prog_no_arg", O_CREATE | O_RDWR);
	if (fd != -1) {
		if (sys_write(fd, prog_buf, file_size) == -1) {
			printk("file write error\n");
			while (1);
		}
	}
	// 写入应用程序ok

	cls_screen();
	console_put_str("[unique9@localhost /]$", 11);

	intr_enable();
	// 不再调度main
	thread_exit(running_thread(), true);

	return 0;
}

void init(void)
{
	int16_t ret_pid = fork();
	if (ret_pid) {	// 父进程
		int status;
		int child_pid;
		//  init 在此处不停地回收孤儿进程
		while (1) {
			child_pid = wait(&status);
			printf("I`m init, My pid is 1, I recieve a child, Its pid is %d, status is %d\n", child_pid, status);
		}
	} else {	// 子进程 
		my_shell();
	}
	panic("init: should not be here");
}