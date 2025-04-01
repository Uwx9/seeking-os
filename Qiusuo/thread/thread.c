#include "thread.h"
#include "string.h"
#include "global.h"
#include "stdint.h"
#include "memory.h"
#include "interrupt.h"
#include "print.h"
#include "debug.h"

#define PG_SIZE 4096

struct task* main_thread;				//主线程PCB
struct list thread_ready_list;			//就绪队列 
struct list thread_all_list;			//所有任务队列
static struct list_elem* thread_tag;	//用于保存队列中的线程结点
extern void switch_to(struct task* cur, struct task* next);

/* 获取当前线程pcb指针 */
struct task* running_thread()
{
	uint32_t esp;
	asm volatile ("mov %%esp, %0;" : "=g"(esp));
	/* 取esp整数部分，即pcb起始地址 */
	return (struct task*)(esp & 0xfffff000);
}


/* 由 kernel_thread 去执行function(func_arg) 
 * 在kernel_thread的视角里,线程栈pop完寄存器,ret到这里,
 * 现在栈顶为一个返回地址,左参数,右参数,仿佛是正常的函数调用一样,但实际是我们精心设计的骗局
 */
static void kernel_thread(thread_func* function, void* func_arg)
{
	/* 执行function前要开中断，避免后面的时钟中断被屏蔽，而无法调度其他线程 */
	intr_enable();
	function(func_arg);
}

/* 初始化线程栈thread_stack，将待执行的函数和参数放到thread_stack中相应的位置 */
void thread_create(struct task* pthread, thread_func* function, void* func_arg)
{
/* 先预留中断使用栈的空间，可见thread.h中定义的结构 */ 
	pthread->self_kstack -= sizeof(struct intr_stack);

/* 再留出线程栈空间，可见thread.h中定义 */
	pthread->self_kstack -= sizeof(struct thread_stack);
	struct thread_stack* kthread_stack = (struct thread_stack*)pthread->self_kstack;	//让kthrad_stack访问线程栈
	/* 手动将kernel_thread返回地址和参数入栈,因为它是ret调用 */
	kthread_stack->eip = kernel_thread;
	kthread_stack->function = function;
	kthread_stack->func_arg = func_arg;
	kthread_stack->ebp = kthread_stack->ebx = kthread_stack->edi = kthread_stack->esi = 0;
}

/* 初始化线程基本信息 */
void init_task(struct task* pthread, char* name, int prio)
{
	memset(pthread, 0, sizeof(*pthread));
	strcpy(pthread->name, name);

	/* 由于把main函数也封装成一个线程，并且它一直是运行的，故将其直接设为TASK_RUNNING */
	if (pthread == main_thread) {
		pthread->status = TASK_RUNNING;
	} else {
		pthread->status = TASK_READY;
	}

	pthread->priority = prio;
/* self_kstack 是线程自己在内核态下使用的栈顶地址 */
	pthread->self_kstack = (uint32_t*)((uint32_t)pthread + PG_SIZE);	 
	pthread->ticks = prio;
	pthread->elapsed_ticks = 0;
	pthread->pgdir = NULL;
	pthread->stack_magic = 0x20050608;

}

/* 创建一优先级为prio的线程，线程名为name，线程所执行的函数是function(func_arg) */ 
/* thread_func和thread_func*是等价的,传递一个函数名会变成函数指针,都指向这个类型的函数*/
struct task* thread_start(char* name, int prio, thread_func function, void* func_arg)
{
/* pcb 都位于内核空间，包括用户进程的pcb也是在内核空间 */ 
	struct task* thread = get_kernel_pages(1);
	init_task(thread, name, prio);
	thread_create(thread, function, func_arg);

	/* 确保之前不在队列中 */
	ASSERT(!elem_find(&thread_ready_list, &thread->general_tag));
	/* 加入就绪线程队列 */
	list_append(&thread_ready_list, &thread->general_tag);

	/* 确保之前不在队列中 */
	ASSERT(!elem_find(&thread_all_list, &thread->all_list_tag));
	/* 加入全部线程队列 */
	list_append(&thread_all_list, &thread->all_list_tag);

	return thread;
}

/* 将kernel中的main函数完善为主线程 */
static void make_main_thread(void)
{
/* 因为main线程早已运行,咱们在loader.S中进入内核时的mov esp,0xc009f000,
 * 就是为其预留pcb的，因此pcb地址为0xc009e000， 
 * 不需要通过get_kernel_page另分配一页
 */ 
	main_thread = running_thread();
	init_task(main_thread, "main", 1);

/* main 函数是当前线程，当前线程不在thread_ready_list中,所以只将其加在thread_all_list中 */ 
	ASSERT(!elem_find(&thread_all_list, &main_thread->all_list_tag));
	list_append(&thread_all_list, &main_thread->all_list_tag);
}

void show_schedule_message(struct task* cur, struct task* next)
{
	put_str("Scheduling from ");
	put_str(cur->name);
	put_str(" to ");
	put_str(next->name);
	put_str("\n");	
}

/* 实现任务调度 */
void schedule()
{
	ASSERT(intr_get_status() == INTR_OFF);
	struct task* cur = running_thread();
	if (cur->status == TASK_RUNNING) {
		// 若此线程只是cpu时间片到了，将其加入到就绪队列尾
		ASSERT(!elem_find(&thread_ready_list, &cur->general_tag));
		list_append(&thread_ready_list, &cur->general_tag);
		cur->ticks = cur->priority;		//重新将当前线程的ticks再重置为其priority 
		cur->status = TASK_READY;
	} else {
		/* 若此线程需要某事件发生后才能继续上cpu运行， 
		不需要将其加入队列，因为当前线程不在就绪队列中 */
	}
	ASSERT(!list_empty(&thread_ready_list));
	thread_tag = NULL;		
/* 将thread_ready_list队列中的第一个就绪线程弹出,准备将其调度上cpu */	
	thread_tag = list_pop(&thread_ready_list);
/* 拿到新的PCB的地址 */
	struct task* next = elem2entry(struct task, general_tag, thread_tag);
	next->status = TASK_RUNNING;

	switch_to(cur, next);

}

/* 当前线程将自己阻塞，标志其状态为stat. */ 
void thread_block(enum task_status statu)
{
	ASSERT(statu == TASK_BLOCKED || statu == TASK_HANGING || statu == TASK_WAITING);
	enum intr_status old_status = intr_disable();
	struct task* cur_thread = running_thread();
	cur_thread->status = statu;
	schedule();
/* 待当前线程被解除阻塞后才继续运行下面的intr_set_status */
	intr_set_status(old_status);
}

/* 将线程pthread解除阻塞 */
void thread_unblock(struct task* pthread)
{
	enum intr_status old_status = intr_disable();
	ASSERT(pthread->status == TASK_BLOCKED || pthread->status == TASK_HANGING || pthread->status == TASK_WAITING);
	if (pthread->status != TASK_READY) {
		ASSERT(!elem_find(&thread_ready_list, &pthread->general_tag));
		if (elem_find(&thread_ready_list, &pthread->general_tag)) {
			PANIC("thread unblock, blocked_thread in ready_list\n");
		}
		list_push(&thread_ready_list, &pthread->general_tag);
		pthread->status = TASK_READY;
	}
	intr_set_status(old_status);
}


/* 初始化线程环境 */
void thread_init(void)
{
	put_str("thread_init start\n");
	list_init(&thread_ready_list);
	list_init(&thread_all_list);
/* 将当前main函数创建为线程 */
	make_main_thread();
	put_str("thread_init done");
}
