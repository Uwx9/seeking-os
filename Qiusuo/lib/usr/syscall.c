#include "syscall.h"
#include "stdint.h"
#include "syscall-init.h"

/* 0个参数的系统调用 */
#define _syscall0(NUMBER) ({		\
	int retval;		\
	asm volatile ("int $0x80" : "=a"(retval) : "a"(NUMBER) : "memory");		\
	retval;		\
})

/* 1个参数的系统调用 */
#define _syscall1(NUMBER, ARG1) ({		\
	int retval;		\
	asm volatile ("int $0x80" : "=a"(retval) : "a"(NUMBER), "b"(ARG1) : "memory");		\
	retval;		\
})

/* 2个参数的系统调用 */
#define _syscall2(NUMBER, ARG1, ARG2) ({		\
	int retval;		\
	asm volatile ("int $0x80" : "=a"(retval) : "a"(NUMBER), "b"(ARG1), "c"(ARG2) : "memory");		\
	retval;		\
})

/* 3个参数的系统调用 */
#define _syscall3(NUMBER, ARG1, ARG2, ARG3) ({		\
	int retval;		\
	asm volatile ("int $0x80" : "=a"(retval) : "a"(NUMBER), "b"(ARG1), "c"(ARG2), "d"(ARG3) : "memory");		\
	retval;		\
})

/* 接口,返回当前任务pid */
uint32_t getpid(void)
{
	return _syscall0(SYS_GETPID);
}

/* 把buf中count个字符写入文件描述符fd */
uint32_t write(int32_t fd, const void* buf, uint32_t count)
{
	return _syscall3(SYS_WRITE, fd, buf, count);
}

/* malloc */
void* malloc(uint32_t size)
{
	// 这里应该类型用uint32_t,eax里的参数我们要的是无符号数, 从syscall返回来的是int,不转换就在这里当int了
	return (void*)_syscall1(SYS_MALLOC, size);
}

/* free */
void free(void* ptr)
{
	_syscall1(SYS_FREE, ptr);
}