#include "stdio.h"
#include "stdint.h"
#include "string.h"
#include "global.h"
#include "./usr/syscall.h"


/* 将整型转换成字符（integer to ascii） */
/* 感觉可以想象成10进制的不断拿到最后一位,实际上应该是一回事,对10进制数处理时它实际都是二进制数,但都能得到正确结果,
 * 那这里不管多少进制都时同理的,要拿到哪个进制下的位就用那个进制去做%或/ */
static void itoa(uint32_t value, char** buf_ptr_addr, uint8_t base)
{
	uint32_t m = value % base;				// 求模，最先掉下来的是最低位
	uint32_t i = value / base;				// 取整
	if (i) {								// 如果倍数不为0，则递归调用
		itoa(i, buf_ptr_addr, base);		// 我们拿到要转化的进制的最高位后再打印,然后返回依次从高位到低位打印
	}
	if (m < 10) {	// 余数为1~9
		*((*buf_ptr_addr)++) = m + '0';
	} else {		// 余数为10及以上
		*((*buf_ptr_addr)++) = m - 10 + 'A';
	}
}

/* 将参数ap按照格式format输出到字符串str，并返回替换后str长度 */
uint32_t vsprintf(char* str, const char* format, va_list ap)
{
	// 注意！字符串是去栈中拿到地址，int和char等直接去栈中拿到值，不需要再通过指针访问，所以printf传参时非字符串直接传值不用&取地址
	char* buf_ptr = str;
	const char* index_ptr = format;
	char index_char = *index_ptr;
	int32_t arg_int;
	char* arg_str;
	while (index_char) {
		if (index_char != '%') {			// 还没遇到%
			*(buf_ptr++) = index_char;
			index_char = *(++index_ptr);
			continue;
		}
		index_char = *(++index_ptr);		// 遇到%
		switch (index_char) {
			case 'x':	
// ap开始是栈中第一个参数的地址,这里调用va_arg,让ap+4指向栈中下一个参数,也就是第一个变参,该参数可能是int*型,访问一个int
				arg_int = va_arg(ap, int);	
// 这里其实用一级指针应该也能输出，但是后面还要用到buf_ptr,所以需要改动它,用一级指针也要想办法改动buf_ptr比较麻烦
				itoa(arg_int, &buf_ptr, 16);	// 把变参的base进制数从高到低依次放到缓冲区
				index_char = *(++index_ptr);		
				break;	

			case 's':
				arg_str = va_arg(ap, char*);
				strcpy(buf_ptr, arg_str);
				buf_ptr += strlen(arg_str);
				index_char = *(++index_ptr);
				break;

			case 'c':
				*(buf_ptr++) = va_arg(ap, char);
				index_char = *(++index_ptr);
				break;

			case 'd':
				arg_int = va_arg(ap, int);
				if (arg_int < 0) {
					arg_int = 0 - arg_int;
					*(buf_ptr++) = '-';
				}
				itoa(arg_int, &buf_ptr, 10);
				index_char = *(++index_ptr);
				break;

		}
	}
	return strlen(str);
}

/* 同printf不同的地方就是字符串不是写到终端，而是写到buf中 */
uint32_t sprintf(char* buf, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	uint32_t retval;
	retval = vsprintf(buf, format, args);
	va_end(args);
	return retval;
}

/* 格式化输出字符串format */ 
uint32_t printf(const char* format, ...)
{
	va_list args;
	va_start(args, format);			// args拿到栈中format的地址并转换为char*类型
	char buf[1204] = {0};
	vsprintf(buf, format, args);
	va_end(args);
	return write(1, buf, strlen(buf));
}