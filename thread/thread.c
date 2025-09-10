#include "thread.h"
#include "string.h"
#include "global.h"
#include "stdint.h"
#include "memory.h"
#include "interrupt.h"
#include "print.h"
#include "debug.h"
#include "process.h"
#include "sync.h"
#include "stdio.h"
#include "file.h"
#include "list.h"

#define PG_SIZE 4096

struct task* idle_thread;				// idle线程
struct task* main_thread;				// 主线程PCB
struct list thread_ready_list;			// 就绪队列 
struct list thread_all_list;			// 所有任务队列
static struct list_elem* thread_tag;	// 用于保存队列中的线程结点
uint8_t pid_bitmap_bits[128] = {0};		// pid 的位图，最大支持1024个pid

extern void switch_to(struct task* cur, struct task* next);
extern void init(void);

/* pid 池 */
struct pid_pool {
	struct bitmap pid_bitmap;	// pid 位图
	uint32_t pid_start;			// 起始 pid 
	struct lock pid_lock;		// 分配 pid 锁
}pid_pool;

/* 系统空闲时运行的线程 */
static void idle(void* arg UNUSED)
{
	while (1) {
		thread_block(TASK_BLOCKED);
		// 执行 hlt 时必须要保证目前处在开中断的情况下
		asm volatile ("sti; hlt;" : : : "memory");
	}
}

/* 初始化pid池 */
static void pid_pool_init(void)
{
	pid_pool.pid_start = 1;
	pid_pool.pid_bitmap.bits = pid_bitmap_bits;
	pid_pool.pid_bitmap.btmp_bytes_len = 128;
	lock_init(&pid_pool.pid_lock);
	
	// 将位图清空
	bitmap_init(&pid_pool.pid_bitmap);
}

/* 分配pid */ 
static pid_t allocate_pid(void)
{
	lock_acquire(&pid_pool.pid_lock);
	int32_t bit_idx = bitmap_scan(&pid_pool.pid_bitmap, 1);
	ASSERT(bit_idx != -1);
	bitmap_set(&pid_pool.pid_bitmap, bit_idx, 1);
	lock_release(&pid_pool.pid_lock);
	return pid_pool.pid_start + bit_idx;
}

/* 释放 pid */
void release_pid(pid_t pid)
{
	lock_acquire(&pid_pool.pid_lock);
	int32_t bit_idx = pid - pid_pool.pid_start;
	bitmap_set(&pid_pool.pid_bitmap, bit_idx, 0);
	lock_release(&pid_pool.pid_lock);
}

pid_t fork_pid(void) 
{
	return allocate_pid();
}

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
	pthread->pid = allocate_pid();
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

	/* 预留标准输入输出*/
	pthread->fd_table[0] = 0;	// 描述符0, 指fd_table下标
	pthread->fd_table[1] = 1;	// 描述符1
	pthread->fd_table[2] = 2;	// 描述符2
	/* 因为不能让文件描述符都指向文件表第0个位置，所以置为-1 */
	uint32_t fd_idx = 3;
	while (fd_idx < MAX_FILES_OPEN_PER_PROC) {
		pthread->fd_table[fd_idx] = -1;
		fd_idx++;
	}
	
	pthread->cwd_inode_nr = 0;	// 以根目录作为默认工作路径
	pthread->parent_pid = -1;	// 任务的父进程默认为−1, 表示没有父进程
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
	init_task(main_thread, "main", 30);

/* main 函数是当前线程，当前线程不在thread_ready_list中,所以只将其加在thread_all_list中 */ 
	ASSERT(!elem_find(&thread_all_list, &main_thread->all_list_tag));
	list_append(&thread_all_list, &main_thread->all_list_tag);
}

void show_schedule_message(struct task* cur, struct task* next)
{
	put_str("Scheduling from ", 0x07);
	put_str(cur->name, 0x07);
	put_str(" to ", 0x07);
	put_str(next->name, 0x07);
	put_str("\n", 0x07);	
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
		/* 若此线程需要某事件发生后才能继续上cpu运行，不需要将其加入队列，因为当前线程不在就绪队列中 */
		
	}
	if (list_empty(&thread_ready_list)) {
		thread_unblock(idle_thread);
	}
	ASSERT(!list_empty(&thread_ready_list));
	thread_tag = NULL;		
/* 将thread_ready_list队列中的第一个就绪线程弹出,准备将其调度上cpu */	
	thread_tag = list_pop(&thread_ready_list);
/* 拿到新的PCB的地址 */
	struct task* next = elem2entry(struct task, general_tag, thread_tag);
	next->status = TASK_RUNNING;

	/* 激活线程或进程的页表，更新tss中的esp0为进程的特权级0的栈 */
	process_activate(next);
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

/* 主动让出cpu，换其他线程运行 */ 
void thread_yield(void) {
	struct task* cur = running_thread();
	enum intr_status old_status = intr_disable();
	ASSERT(!elem_find(&thread_ready_list, &cur->general_tag));
	list_append(&thread_ready_list, &cur->general_tag);
	cur->status = TASK_READY;
	schedule();
	intr_set_status(old_status);
}

/* 以填充空格的方式输出buf, 对齐 */
static void pad_print(char* buf, int32_t buf_len, void* ptr, char format)
{
	memset(buf, 0, buf_len);
	uint8_t out_pad_0idx = 0;
	switch (format) {
		case 's':
			out_pad_0idx = sprintf(buf, "%s", ptr);
			break;
		case 'd':
			out_pad_0idx = sprintf(buf, "%d", *(int16_t*)ptr);
			break;
		case 'x':
			out_pad_0idx = sprintf(buf, "%x", *(uint32_t*)ptr);
	}
	while (out_pad_0idx < buf_len) {
		buf[out_pad_0idx] = ' ';
		out_pad_0idx++;
	}
	sys_write(stdout_no, buf, buf_len - 1);	// 为啥要-1啊, 实际是15个字节对齐
}

/* 用于在list_traversal函数中的回调函数，用于针对线程队列的处理 */
static bool elem2thread_info(struct list_elem* pelem, int arg UNUSED)
{
	// 都是对齐输出
	// 先打印自己的pid，
	struct task* pthread = elem2entry(struct task, all_list_tag, pelem);
	char out_pad[16] = {0};
	pad_print(out_pad, 16, &pthread->pid, 'd');	// 对齐pid, 将pid放进来，后面的字节补空格
	
	// 父进程的pid
	if (pthread->parent_pid == -1) {
		pad_print(out_pad, 16, "NULL", 's');
	} else {
		pad_print(out_pad, 16, &pthread->parent_pid, 'd');
	}

	// 自己的status
	switch (pthread->status) {
		case 0:
			pad_print(out_pad, 16, "RUNNING", 's');
			break;
		case 1:
			pad_print(out_pad, 16, "READY", 's');
			break;
		case 2:
			pad_print(out_pad, 16, "BLOCKED", 's');
			break;
		case 3:
			pad_print(out_pad, 16, "WAITING", 's');
			break;
		case 4:
			pad_print(out_pad, 16, "HANGING", 's');
			break;
		case 5:
			pad_print(out_pad, 16, "OVER", 's');
	}

	// 任务总运行ticks
	pad_print(out_pad, 16, &pthread->elapsed_ticks, 'x');

	// 打印任务名称
	memset(out_pad, 0, 16);
	ASSERT(strlen(pthread->name) < 16);	// name最多15字节吧，这样\n才能放在out_pad里面
	memcpy(out_pad, pthread->name, strlen(pthread->name));
	strcat(out_pad, "\n");
	sys_write(stdout_no, out_pad, strlen(out_pad));
	return false;	 // 此处返回false是为了迎合主调函数list_traversal, 只有回调函数返回false时才会继续调用此函数 
}

/* 打印任务列表 */
void sys_ps(void)
{
	char* ps_title = "PID            PPID           STAT           TICKS          COMMAND\n";
	sys_write(stdout_no, ps_title, strlen(ps_title));
	list_traversal(&thread_all_list, elem2thread_info, 0);
}

/* 回收thread_over的pcb和页目录，并将其从调度队列中去除 */
void thread_exit(struct task* thread_over, bool need_schedule)
{
	// 在关中断的情况下调用
	intr_disable();
	thread_over->status = TASK_OVER;
	
	/* 如果 thread_over 不是当前线程，就有可能还在就绪队列中，将其从中删除 */
	if (elem_find(&thread_ready_list, &thread_over->general_tag)) {
		list_remove(&thread_over->general_tag);
	}
	if (thread_over->pgdir){	// 如是进程，回收进程的页目录
		mfree_page(PF_KERNEL, thread_over->pgdir, 1);
	}

	// 从 all_thread_list 中去掉此任务
	list_remove(&thread_over->all_list_tag);

	// 归还 pid, 这里应该是要写在回收pcb页上面
	release_pid(thread_over->pid);
	
	// 回收 pcb所在的页，主线程的pcb不在堆中，跨过
	if (thread_over != main_thread) {
		mfree_page(PF_KERNEL, thread_over, 1);
	}

	// 如果需要下一轮调度则主动调用schedule
	if (need_schedule) {
		schedule();
		PANIC("thread_exit: should not be hrer\n");	// thread_over已经不会被调度了
	}
}

/* 比对任务的pid */
static bool pid_check(struct list_elem* pelem, int32_t pid)
{
	struct task* pthread = elem2entry(struct task, all_list_tag, pelem);
	if (pthread->pid == pid) {
		return true;
	}
	return false;
}

/* 根据pid找pcb，若找到则返回该pcb，否则返回NULL */
struct task* pid2thread(int32_t pid)
{
	struct list_elem* pelem = list_traversal(&thread_all_list, pid_check, pid);
	if (pelem == NULL) {	// 遍历完没有找到pid
		return NULL;
	}
	struct task* thread = elem2entry(struct task, all_list_tag, pelem);
	return thread;
}

/* 初始化线程环境 */
void thread_init(void)
{
	put_str("thread_init start\n", 0x07);
	list_init(&thread_ready_list);
	list_init(&thread_all_list);
	pid_pool_init();

	/* 先创建第一个用户进程:init */
	// 放在第一个初始化，这是第一个进程，init进程的pid为1
	process_execute(init, "init");

	/* 将当前main函数创建为线程 */
	make_main_thread();
	/* 创建 idle线程 */
	idle_thread = thread_start("idle", 10, idle, NULL);
	put_str("thread_init done\n", 0x07);
}

