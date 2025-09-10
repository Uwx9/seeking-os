#include "init.h"
#include "print.h"
#include "interrupt.h"
#include "../device/timer.h"
#include "memory.h"
#include "thread.h"
#include "console.h"
#include "keyboard.h"
#include "tss.h"
#include "syscall-init.h"
#include "ide.h"
#include "fs.h"

/* 初始化所有模块 */
void init_all()
{
    put_str("init_all\n", 0x07);
    idt_init();
    timer_init();
    mem_init();
	thread_init();	
	console_init();
	keyboard_init();
	tss_init();
	syscall_init();
	ide_init();
	filesys_init();
}