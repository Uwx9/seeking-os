#ifndef __LIB_USR_STDIO_H
#define __LIB_USR_STDIO_H
#include "stdint.h"

typedef char* va_list;
#define va_start(ap, v) ap = (va_list)&v	// ap拿到第一个参数的地址
#define va_arg(ap, t) *((t*)(ap += 4))		// ap指向下一个t类型参数，并返回其值
#define va_end(ap) ap = NULL				// 清除ap

uint32_t vsprintf(char* str, const char* format, va_list ap);
uint32_t sprintf(char* buf, const char* format, ...);
uint32_t printf(const char* format, ...);

#endif 