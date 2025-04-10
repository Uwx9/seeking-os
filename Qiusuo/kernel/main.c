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

void k_thread_a(void*);
void k_thread_b(void*);
void k_thread_aa(void*);
void k_thread_bb(void*);
void u_prog_a(void);
void u_prog_b(void);

int ua = 0, ub =0;

int main(void)
{
	put_str("I'm kernel!\n");
	init_all();
	
	thread_start("k_thread_aa", 31, k_thread_aa, "A_ ");	
	thread_start("k_thread_bb", 31, k_thread_bb, "B_ ");
	// 由线程执行函数start_process,它再去模拟从中断返回,进入线程。
	process_execute(u_prog_a, "user_prog_a");
	process_execute(u_prog_b, "user_prog_b");

	intr_enable();
	while (1); 
	return 0;
}


void k_thread_aa(void* arg)
{
	char* parg = arg;
	while (1) {
		console_put_str(" ua = 0x:");
	 	console_put_int(ua);
	}
}

void k_thread_bb(void* arg)
{
	char* parg = arg;
	while (1) {
		console_put_str(" ub = 0x:");
	 	console_put_int(ub);
	}	
}



void k_thread_a(void* arg)
{
	char* parg = arg;
	while (1) {
		enum intr_status old_status = intr_disable();
		if (!ioq_empty(&kbd_buf)) {
			console_put_str(parg);
			char ch = ioq_getchar(&kbd_buf);
			console_put_char(ch);
			console_put_char(' ');
		}
		intr_set_status(old_status);
	}
}

void k_thread_b(void* arg)
{
	char* parg = arg;
	while (1) {
		enum intr_status old_status = intr_disable();
		if (!ioq_empty(&kbd_buf)) {
			console_put_str(parg);
			char ch = ioq_getchar(&kbd_buf);
			console_put_char(ch);
			console_put_char(' ');
		}
		intr_set_status(old_status);
	}
}

void u_prog_a(void){
		while (1) {
			ua++;			
		}
}

void u_prog_b(void){
		while (1) {
			ub++;			
		}
}