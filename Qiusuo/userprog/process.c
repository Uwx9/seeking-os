#include "process.h"
#include "../thread/thread.h"
#include "stdint.h"
#include "global.h"
#include "../kernel/memory.h"
#include "debug.h"
#include "tss.h"
#include "./userprog.h"
#include "print.h" 
#include "interrupt.h"
#include "list.h"
#include "string.h"
#include "memory.h"

extern void intr_exit(void);
extern struct list thread_ready_list;
extern struct list thread_all_list;

/* 构建用户进程初始上下文信息 */
void start_process(void* filename_)
{
	void* function = filename_;
	struct task* cur = running_thread();
	cur->self_kstack += sizeof(struct thread_stack);
	struct intr_stack* proc_stack = (struct intr_stack*)cur->self_kstack;

	proc_stack->edi = proc_stack->esi = proc_stack->ebp = proc_stack->esp_dummy = 0;
	proc_stack->eax = proc_stack->ebx = proc_stack->ecx = proc_stack->edx = 0;
	proc_stack->gs = 0;
	
	proc_stack->cs = SELECTOR_U_CODE;
	proc_stack->ds = proc_stack->es = proc_stack->fs = proc_stack->ss = SELECTOR_U_DATA;
	proc_stack->eip = function;
	proc_stack->eflags = EFLAGS_IF_1 | EFLAGS_IOPL_0 | EFLAGS_MBS;
	
	// USER_STACK3_VADDR是用户进程栈的最低地址(栈顶)，为它分配一个物理页(用进程自己的页目录表),再vaddr + 4K得到栈底的位置
	proc_stack->esp = (void*)((uint32_t)get_a_page(PF_USER, USER_STACK3_VADDR) + PG_SIZE);	// 给用户进程准备的
	asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g"(proc_stack) : "memory");
}

/* 激活页表 */
void page_dir_activate(struct task* pthread)
{
/******************************************************** 
* 执行此函数时，当前任务可能是线程。 
* 之所以对线程也要重新安装页表，原因是上一次被调度的可能是进程， 
* 否则不恢复页表的话，线程就会使用进程的页表了。 
********************************************************/
	uint32_t page_dir_phyaddr = 0x100000;
	
	// 用户态进程有自己的页目录表
	if (pthread->pgdir) {
		page_dir_phyaddr = addr_v2p((uint32_t)pthread->pgdir);
	}
	
	//更新页目录表
	asm volatile ("movl %0, %%cr3" : : "r"(page_dir_phyaddr) : "memory");
}

/* 激活线程或进程的页表，更新tss中的esp0为进程的特权级0的栈 */
void process_activate(struct task* pthread)
{
	ASSERT(pthread != NULL);
	/* 激活该进程或线程的页表 */
	page_dir_activate(pthread);

	/* 内核线程特权级本身就是0，处理器进入中断时并不会从tss中获取0特权级栈地址，故不需要更新esp0*/
	if (pthread->pgdir != NULL) {
		/* 更新该进程的esp0，用于此进程被中断时保留上下文 */
		update_tss_esp(pthread);
	}
}

/* 创建页目录表，将当前页表的表示内核空间的pde复制,成功则返回页目录的虚拟地址，否则返回NULL */
uint32_t* create_page_dir(void)
{
	uint32_t* page_dir_vaddr = get_kernel_pages(1);
	if (page_dir_vaddr == NULL) {
		put_str("failed to get_kernel_pages for page_dir");
		return NULL;
	}

	//将用户页目录的高1G虚拟地址与内核的一致
	memcpy((void*)((uint32_t)page_dir_vaddr + 0x300 * 4), (void*)(0xfffff000 + 0x300 * 4), 1024);

	//设置用户页目录最后一项为自己页目录的基址
	uint32_t new_page_dir_phyaddr = addr_v2p((uint32_t)page_dir_vaddr); 
	page_dir_vaddr[1023] = new_page_dir_phyaddr | PG_US_U | PG_RW_W | PG_P_1; 
	return page_dir_vaddr;
}

/* 创建用户进程虚拟地址位图 */
void create_user_vaddr_bitmap(struct task* user_prog)
{
		user_prog->userprog_vaddr.vaddr_start = USER_VADDR_START;
		// 当位图大小为4K时只用一页，大小为4K-1时要用多一页，由DIV来保证。它可以使除法有余数时向上取整
		uint32_t bitmap_pg_cnt = DIV_ROUND_UP((0xc0000000 - USER_VADDR_START) / PG_SIZE / 8, PG_SIZE);
		user_prog->userprog_vaddr.vaddr_bitmap.bits = get_kernel_pages(bitmap_pg_cnt);
		user_prog->userprog_vaddr.vaddr_bitmap.btmp_bytes_len = (0xc0000000 - USER_VADDR_START) / PG_SIZE / 8;
		bitmap_init(&user_prog->userprog_vaddr.vaddr_bitmap);
}

/* 创建用户进程 */
void process_execute(void* filename, char* name)
{
	/* pcb 内核的数据结构，由内核来维护进程信息，因此要在内核内存池中申请 */
	struct task* thread = get_kernel_pages(1);
	init_task(thread, name, default_prio);
	create_user_vaddr_bitmap(thread);
	thread_create(thread, start_process, filename);
	thread->pgdir = create_page_dir();
	block_descs_init(thread->u_block_descs);

	
	enum intr_status old_status = intr_disable();
	ASSERT(!elem_find(&thread_ready_list, &thread->general_tag));
	list_append(&thread_ready_list, &thread->general_tag);
	ASSERT(!elem_find(&thread_all_list, &thread->all_list_tag));
	list_append(&thread_all_list, &thread->all_list_tag);
	intr_set_status(old_status);
}
