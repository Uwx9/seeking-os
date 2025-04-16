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

void k_thread_a(void*);
void k_thread_b(void*);
void k_thread_aa(void*);
void k_thread_bb(void*);
void u_prog_a(void);
void u_prog_b(void);

int prog_a_pid = 0, prog_b_pid =0;

int main(void)
{
	put_str("I'm kernel!\n");
	init_all();
	intr_enable();
	
	// 由线程执行函数start_process,它再去模拟从中断返回,进入线程。
	// 最开始在u_prog_a中调用sys_getpid没有得到正确结果,这时候esp在进程的栈中为0xbfffffff下一点,没有task的信息,
	// 要使用接口getpid进入中断,因该是硬件帮我们切换了栈(tss的esp0我们已经保存为进程PCB的0特权级栈),这时候才能访问进程PCB中的信息。 
	/* 刚才有点担心用户进程进中断后，esp不是在内核栈上面一页的基址，但实际我们call子函数时已经保存上下文，esp已经在用户进程PCB中，这很好，
	 * 并且保存上下文时当然保存cs，eip，ss，esp（都是进程的）也可以利用它们访问进程的资源。*/
	process_execute(u_prog_a, "user_prog_a");
	process_execute(u_prog_b, "user_prog_b");

	
	// console_put_str("main_pid is 0x:");
	// console_put_int(sys_getpid());
	// console_put_char('\n');
	thread_start("k_thread_a", 31, k_thread_a, "I am thread a! ");	
	thread_start("k_thread_b", 31, k_thread_b, "I am thread b! ");
	
	while (1); 
	return 0;
}


void k_thread_a(void* arg)
{

	void* addr1 = sys_malloc(256); 
    void* addr2 = sys_malloc(255); 
    void* addr3 = sys_malloc(254); 
    console_put_str(" thread_a malloc addr:0x"); 
    console_put_int((int)addr1); 
    console_put_char(',');
	console_put_int((int)addr2); 
    console_put_char(','); 
    console_put_int((int)addr3); 
    console_put_char('\n'); 
    int cpu_delay = 1000000; 
    while(cpu_delay-- > 0); 
    sys_free(addr1); 
    sys_free(addr2); 
    sys_free(addr3); 
    while(1); 

}

void k_thread_b(void* arg)
{

	void* addr1 = sys_malloc(256); 
    void* addr2 = sys_malloc(255); 
    void* addr3 = sys_malloc(254); 
    console_put_str(" thread_b malloc addr:0x"); 
    console_put_int((int)addr1); 
    console_put_char(',');
	console_put_int((int)addr2); 
    console_put_char(','); 
    console_put_int((int)addr3); 
    console_put_char('\n'); 
    int cpu_delay = 100000; 
    while(cpu_delay-- > 0); 
    sys_free(addr1); 
    sys_free(addr2); 
    sys_free(addr3); 
    while(1); 

}

void u_prog_a(void){
	void* addr1 = malloc(256); 
    void* addr2 = malloc(255); 
    void* addr3 = malloc(254); 
	printf(" prog_a malloc addr:0x%x,0x%x,0x%x\n", (int)addr1, (int)addr2, (int)addr3); 
    int cpu_delay = 100000; 
    while(cpu_delay-- > 0); 
    free(addr1); 
    free(addr2); 
    free(addr3); 
    while(1);
}

void u_prog_b(void){
	void* addr1 = malloc(256); 
    void* addr2 = malloc(255); 
    void* addr3 = malloc(254); 
	printf(" prog_b malloc addr:0x%x,0x%x,0x%x\n", (int)addr1, (int)addr2, (int)addr3); 
    int cpu_delay = 100000; 
    while(cpu_delay-- > 0); 
    free(addr1); 
    free(addr2); 
    free(addr3); 
    while(1);

}