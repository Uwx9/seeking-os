#include "console.h"
#include "print.h"
#include "thread.h"
#include "stdint.h"
#include "sync.h"

static struct lock console_lock;		//控制台锁

/* 初始化终端 */
void console_init()
{
	lock_init(&console_lock);
}

/* 获取终端 */
void console_acquire()
{
	lock_acquire(&console_lock);
}

/* 释放终端 */ 
void console_realse()
{
	lock_release(&console_lock);
}

uint32_t str_color = 0x07;
uint32_t char_color = 0x07;

/* 终端中输出字符串 */
void console_put_str(char* str, uint32_t char_attr)	// 第二个参数看来有点多余了,之前没想到用全局变量, 需要修改一下
{
	console_acquire();
	char_attr = str_color;
	put_str(str, char_attr);
	console_realse();
}

/* 终端中输出字符 */
void console_put_char(uint8_t char_asci, uint32_t char_attr)
{
	console_acquire();
	char_attr = char_color;
	put_char(char_asci, char_attr);
	console_realse();
}

/* 终端中输出十六进制整数 */
void console_put_int(uint32_t num)
{
	console_acquire();
	put_int(num);
	console_realse();
}