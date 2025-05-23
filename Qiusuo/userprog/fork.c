#include "fork.h"
#include "string.h"
#include "global.h"
#include "thread.h"
#include "process.h"
#include "debug.h"
#include "file.h"
#include "inode.h"
#include "interrupt.h"
#include "list.h"
#include "pipe.h"

extern void intr_exit(void);

/* 将父进程的pcb拷贝给子进程 */
static int32_t copy_pcb_vaddrbitmap_stack0(struct task* child_thread, struct task* parent_thread)
{
	/* a 复制pcb所在的整个页，里面包含进程pcb信息及特级0极的栈，里面包含了返回地址 */
	memcpy(child_thread, parent_thread, PG_SIZE);
	// 再单独修改
	child_thread->pid = fork_pid();
	child_thread->elapsed_ticks = 0;
	child_thread->status = TASK_READY;
	child_thread->ticks = child_thread->priority;	//为新进程把时间片充满
	child_thread->parent_pid = parent_thread->pid;
	child_thread->general_tag.next = child_thread->general_tag.prev = NULL;
	child_thread->all_list_tag.next = child_thread->all_list_tag.prev = NULL;
	block_descs_init(child_thread->u_block_descs);
	/* b 复制父进程的虚拟地址池的位图, 为了不丢页，要向上取整将所有页保留 */
	uint32_t bitmap_pg_cnt = DIV_ROUND_UP((0xc0000000 - USER_VADDR_START) / PG_SIZE / 8, PG_SIZE);
	void* vaddr_bitmap = get_kernel_pages(bitmap_pg_cnt);

/* 此时 child_thread->userprog_vaddr.vaddr_bitmap.bits 还是指向父进程虚拟地址的位图地址      
 * 下面将child_thread->userprog_vaddr.vaddr_bitmap.bits 指向自己的位图vaddr_btmp
 * */ 	
	memcpy(vaddr_bitmap, child_thread->userprog_vaddr.vaddr_bitmap.bits, bitmap_pg_cnt * PG_SIZE);
	child_thread->userprog_vaddr.vaddr_bitmap.bits = vaddr_bitmap;
	// 调试用
	ASSERT(strlen(child_thread->name) < 11);
	// pcb.name 的长度是16，为避免下面strcat越界
	strcat(child_thread->name, "_fork");
	return 0;
}

/* 复制父进程的进程体（代码和数据）及用户栈到子进程 */
static void copy_body_stack3(struct task* child_thread, struct task* parent_thread, void* buf_page)
{
	uint8_t* vaddr_bitmap = parent_thread->userprog_vaddr.vaddr_bitmap.bits;
	uint32_t bitmap_bytes_len = parent_thread->userprog_vaddr.vaddr_bitmap.btmp_bytes_len;
	uint32_t vaddr_start = parent_thread->userprog_vaddr.vaddr_start;
	uint32_t idx_byte = 0;
	uint32_t idx_bit = 0;
	uint32_t prog_vaddr = 0;
	/* 在父进程的用户空间中查找已有数据的页 */
	while (idx_byte < bitmap_bytes_len) {
		if (vaddr_bitmap[idx_byte]) {
			idx_bit = 0;
			while (idx_bit < 8) {
				if ((BITMAP_MASK << idx_bit) & vaddr_bitmap[idx_byte]) {	// 看这个字节哪一位为1
					prog_vaddr = (idx_byte * 8 + idx_bit) * PG_SIZE + vaddr_start;
					/* 下面的操作是将父进程用户空间中的数据通过内核空间做中转，最终复制到子进程的用户空间 */
					/* a 将父进程在用户空间中的数据复制到内核缓冲区buf_page，目的是下面切换到子进程的页表后，还能访问到父进程的数据*/
					memcpy(buf_page, (void*)prog_vaddr, PG_SIZE);
					/* b 将页表切换到子进程，目的是避免下面申请内存的函数 将pte及pde安装在父进程的页表中 */
					page_dir_activate(child_thread);
					/* c 申请虚拟地址prog_vaddr, 为它安装物理页 */
					get_a_page_without_opvaddrbitmap(PF_USER, prog_vaddr);
					/* d 从内核缓冲区中将父进程数据复制到子进程的用户空间 */
					memcpy((void*)prog_vaddr, buf_page, PG_SIZE);
					/* e 恢复父进程页表 */
					page_dir_activate(parent_thread);
				}
				idx_bit++;
			}
		}
		idx_byte++;
	}
}

/* 为子进程构建thread_stack和修改返回值 */
static int32_t build_child_stack(struct task* child_thread)
{
	/* a 使子进程pid返回值为0 */
	// 获取子进程0级栈栈顶
	struct intr_stack* intr_0_stack = (struct intr_stack*)((uint32_t)child_thread + PG_SIZE - sizeof(struct intr_stack));
	// 修改子进程的返回值为0
	intr_0_stack->eax = 0;

	/* b 为 switch_to 构建 struct thread_stack，将其构建在紧临intr_stack之下的空间*/ 
	uint32_t* ret_addr_in_thread_stack = (uint32_t*)intr_0_stack - 1;

	/* ebp 在 thread_stack 中的地址便是当时的esp（0级栈的栈顶），即esp为"(uint32_t*)intr_0_stack - 5" */
	uint32_t* ebp_ptr_in_thread_stack = (uint32_t*)intr_0_stack - 5;

	/* switch_to 的返回地址更新为intr_exit，直接从中断返回, 
	 * 此时栈指向0级栈栈顶，就仿佛父进程返回一样返回，
	 * child的0级栈的返回地址和parent一样，能保障返回后都从fork之后开始执行
	 *  */
	*ret_addr_in_thread_stack = (uint32_t)intr_exit;

	/* 把构建的thread_stack的栈顶作为switch_to恢复数据时的栈顶 */
	child_thread->self_kstack = ebp_ptr_in_thread_stack;
	return 0;
}

/* 更新inode打开数, 将这个进程打开的文件的打开次数 + 1 */
static void update_inode_open_cnts(struct task* thread)
{
	int32_t local_idx = 3, global_idx = 0;
	while (local_idx < MAX_FILES_OPEN_PER_PROC) {
		global_idx = thread->fd_table[local_idx];
		ASSERT(global_idx < MAX_FILES_OPEN);
		if (global_idx != -1) {
			if (is_pipe(local_idx)) {
				file_table[global_idx].fd_pos++;	// 管道的打开次数
			} else {
				file_table[global_idx].fd_inode->i_open_cnts++;
			}
		}
		local_idx++;
	}
}

/* 拷贝父进程本身所占资源给子进程 */
static int32_t copy_process(struct task* child_thread, struct task* parent_thread) 
{
	void* buf_page = get_kernel_pages(1);
	if (buf_page == NULL) {
		return -1;
	}

	/* a 复制父进程的pcb、虚拟地址位图、内核栈到子进程 */
	if (copy_pcb_vaddrbitmap_stack0(child_thread, parent_thread) == -1) {
		return -1;
	}

	/* b 为子进程创建页表，此页表仅包括内核空间 */	
	child_thread->pgdir = create_page_dir();
	if (child_thread->pgdir == NULL) {
		return -1;
	}

	/* c 复制父进程进程体及用户栈给子进程 */
	copy_body_stack3(child_thread, parent_thread, buf_page);

	/* d 构建子进程thread_stack和修改返回值pid */
	build_child_stack(child_thread);

	/* e 更新文件inode的打开数 */
	update_inode_open_cnts(child_thread);

	mfree_page(PF_KERNEL, buf_page, 1);
	return 0;

}

/* fork 子进程，内核线程不可直接调用 */
pid_t sys_fork(void)
{
	struct task* parent_thread = running_thread();
	struct task* child_thread = (struct task*)get_kernel_pages(1);
	if (child_thread == NULL) {
		return -1;
	}
	ASSERT(INTR_OFF == intr_get_status() && parent_thread->pgdir != NULL);
	if (copy_process(child_thread, parent_thread) == -1) {
		return -1;
	}

	/* 添加到就绪线程队列和所有线程队列，子进程由调试器安排运行 */
	ASSERT(!elem_find(&thread_ready_list, &child_thread->general_tag));
	list_append(&thread_ready_list, &child_thread->general_tag);
	ASSERT(!elem_find(&thread_all_list, &child_thread->all_list_tag));
	list_append(&thread_all_list, &child_thread->all_list_tag);

	return child_thread->pid;	// 父进程返回子进程的pid
}