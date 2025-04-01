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
	lock_realse(&console_lock);
}

/* 终端中输出字符串 */
void console_put_str(char* str)
{
	console_acquire();
	put_str(str);
	console_realse();
}

/* 终端中输出字符 */
void console_put_char(uint8_t char_asci)
{
	console_acquire();
	put_char(char_asci);
	console_realse();
}

/* 终端中输出十六进制整数 */
void console_put_int(uint32_t num)
{
	console_acquire();
	put_int(num);
	console_realse();
}