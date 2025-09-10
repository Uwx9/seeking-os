#include "stdio-kernel.h"
#include "../stdio.h"
#include "stdint.h"
#include "console.h"
#include "global.h"
#include "string.h"

/* 供内核使用的格式化输出函数 */
uint32_t printk(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	char buf[1024] = {0};
	vsprintf(buf, format, args);
	va_end(args);
	console_put_str(buf, 0x07);
	return strlen(buf);
}

/* 输出一个字符 */
void sys_putchar(const char chr)
{
	console_put_char(chr, 0x07);
}