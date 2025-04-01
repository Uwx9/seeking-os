#include "print.h"
#include "init.h"
#include "thread.h"
#include "interrupt.h"
#include "memory.h"
#include "list.h"
#include "console.h"

void k_thread_a(void*);
void k_thread_b(void*);

int main(void)
{
	put_str("I'm kernel!\n");
	init_all();
	
	thread_start("k_thread_a", 1, k_thread_a, "arg_A ");	
	thread_start("k_thread_b", 1, k_thread_b, "arg_B ");
	intr_enable();
	
	while (1) {
		console_put_str("main ");
	}
	return 0;
}

void k_thread_a(void* arg)
{
	char* parg = arg;
	while (1) {
		console_put_str(parg);
	}
}

void k_thread_b(void* arg)
{
	char* parg = arg;
	while (1) {
		console_put_str(parg);
	}
}