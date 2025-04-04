#include "print.h"
#include "init.h"
#include "thread.h"
#include "interrupt.h"
#include "memory.h"
#include "list.h"
#include "console.h"
#include "keyboard.h"
#include "ioqueue.h"

void k_thread_a(void*);
void k_thread_b(void*);

int main(void)
{
	put_str("I'm kernel!\n");
	init_all();
	
	thread_start("consumer_a", 31, k_thread_a, "A_ ");	
	thread_start("k_thread_b", 31, k_thread_b, "B_ ");
	intr_enable();
	
	while (1); //{
		//console_put_str("main ");
	//}
	return 0;
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
