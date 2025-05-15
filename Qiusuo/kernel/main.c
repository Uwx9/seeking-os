#include "print.h"
#include "init.h"
#include "thread.h"
#include "interrupt.h"
#include "memory.h"
#include "list.h"
#include "console.h"
#include "keyboard.h"
#include "ioqueue.h"
#include "process.h"
#include "syscall-init.h"
#include "syscall.h"
#include "../lib/stdio.h"
#include "fs.h"
#include "string.h"
#include "dir.h"
#include "shell.h"
#include "debug.h"

void init(void);

int main(void)
{
	put_str("I'm kernel!\n", 0x07);
	init_all();
	cls_screen();
	str_color = 12;
	console_put_str("[unique9@localhost /]$", 11);
	str_color = 7;
	intr_enable();
	

	while(1);
	return 0;
}

void init(void)
{
	int16_t ret_pid = fork();
	if (ret_pid) {
		while (1);
	} else {
		my_shell();
	}
	PANIC("init: should not be here");
}