#ifndef __LIB_USR_SYSCALL_H
#define __LIB_USR_SYSCALL_H
#include "stdint.h"

enum SYS_NR{
	SYS_GETPID,
	SYS_WRITE,
	SYS_MALLOC,
	SYS_FREE
};

uint32_t getpid(void);
uint32_t write(int32_t fd, const void* buf, uint32_t count);
void* malloc(uint32_t size);
void free(void* ptr);
#endif 