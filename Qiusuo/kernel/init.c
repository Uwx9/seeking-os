#include "init.h"
#include "print.h"
#include "interrupt.h"
#include "../device/timer.h"
#include "memory.h"
#include "thread.h"
#include "console.h"
#include "keyboard.h"
#include "tss.h"

/* 初始化所有模块 */
void init_all()
{
    put_str("init_all\n");
    idt_init();
    timer_init();
    mem_init();
	thread_init();	
	console_init();
	keyboard_init();
	tss_init();
}