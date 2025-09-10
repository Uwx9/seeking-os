#include "pipe.h"
#include "global.h"
#include "fs.h"
#include "file.h"
#include "ioqueue.h"
#include "string.h"

extern struct file file_table[MAX_FILES_OPEN];

/* 判断文件描述符local_fd是否是管道 */
bool is_pipe(uint32_t local_fd)
{
	uint32_t global_fd = fd_local2global(local_fd);
	return file_table[global_fd].fd_flag == PIPE_FLAG;
}

/* 创建管道，并将管道的描述符放在pipefd里，成功返回0，失败返回−1 */
int32_t sys_pipe(int32_t pipefd[2])
{
	int32_t global_fd = get_free_slot_in_global();

	/* 申请一页内核内存做环形缓冲区 */
	file_table[global_fd].fd_inode = get_kernel_pages(1);
	if (file_table[global_fd].fd_inode == NULL) {
		return -1;
	}

	/* 初始化环形缓冲区 */
	ioqueue_init((struct ioqueue*)file_table[global_fd].fd_inode);	
	// 缓冲区就是一个ioqueue结构体, 可以很大毕竟给了4k
	
	/* 将 fd_flag 复用为管道标志 */
	file_table[global_fd].fd_flag = PIPE_FLAG;
	
	/* 将 fd_pos 复用为管道打开数 */
	file_table[global_fd].fd_pos = 2;
	pipefd[0] = pcb_fd_install(global_fd);
	pipefd[1] = pcb_fd_install(global_fd);
	return 0;
}

/* 从管道中读数据 */
uint32_t pipe_read(int32_t fd, void* buf, uint32_t count)
{
	char* buffer = buf;
	uint32_t bytes_read = 0;
	uint32_t global_fd = fd_local2global(fd);
	
	/* 获取管道的环形缓冲区 */
	struct ioqueue* ioq = (struct ioqueue*)file_table[global_fd].fd_inode;

	/* 选择较小的数据读取量, 避免阻塞, ioq_len最大为buffsize - 1, 意思就是极限就把缓冲区刚好读完, 不多读就不会调用getcahr导致阻塞 */
	uint32_t ioq_len = ioq_lenth(ioq);
	uint32_t size = ioq_len > count ? count : ioq_len;
	while (bytes_read < size) {
		*buffer = ioq_getchar(ioq);
		bytes_read++;
		buffer++;
	}
	return bytes_read;
}

/* 往管道中写数据, 返回bytes_write */
uint32_t pipe_write(int32_t fd, void* buf, uint32_t count)
{
	char* buffer = buf;
	uint32_t bytes_write = 0;
	uint32_t global_fd = fd_local2global(fd);
	
	/* 获取管道的环形缓冲区 */
	struct ioqueue* ioq = (struct ioqueue*)file_table[global_fd].fd_inode;

	/* 选择较小的写入量，避免阻塞, 极限是刚好写满, 不多写一个, 写满后就不会调用putchar导致阻塞 */
	uint32_t ioq_left = bufsize - ioq_lenth(ioq);
	uint32_t size = ioq_left > count ? count : ioq_left;
	while (bytes_write < size) {
		ioq_putchar(ioq, *buffer);
		bytes_write++;
		buffer++;
	}
	return bytes_write;
}

/* 将文件描述符old_local_fd重定向为new_local_fd */
void sys_fd_redirect(uint32_t old_local_fd, uint32_t new_local_fd)
{
	struct task* cur = running_thread();
	/* 针对恢复标准描述符 */
	if (new_local_fd < 3) {	// 这里恢复标准输入输出,指向键盘和显示器,不再指向管道
		cur->fd_table[old_local_fd] = new_local_fd;
	} else {
		uint32_t new_global_fd = cur->fd_table[new_local_fd];
		cur->fd_table[old_local_fd] = new_global_fd;
	}
}
