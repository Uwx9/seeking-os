#include "sync.h"
#include "interrupt.h"
#include "debug.h"
#include "thread.h"

/* 初始化信号量 */
void sema_init(struct semaphore* sem, uint8_t vaule)
{
	list_init(&sem->waiters);			//初始化信号量的等待队列
	sem->value = vaule;
}

/* 初始化锁plock */
void lock_init(struct lock* plock)
{
	plock->holder = NULL;
	sema_init(&plock->semaphore, 1);	//信号量初值为1
	plock->holder_repeat_nr = 0;
}

/* 信号量down操作 */
void sema_down(struct semaphore* psem)
{
	/* 关中断来保证原子操作 */
	enum intr_status old_status = intr_disable();
	/* 这里用while是因为可能没有释放锁却唤醒了线程吗，所以要再次判断 */
	/* 原因是用if的话，若b线程即将被持有锁的a线程唤醒，若此时c线程来抢走锁，没有while的话b线程不会再判断value是否为0 */
	while (psem->value == 0) {
		/* 当前线程调用down说明还没被阻塞，不应该在waiters队列中 */
		ASSERT(!elem_find(&psem->waiters, &running_thread()->general_tag));
		if (elem_find(&psem->waiters, &running_thread()->general_tag)) {
			PANIC("sema down: thread blocked has been in waiters_list\n");
		}
/* 若信号量的值等于0，则当前线程把自己加入该锁的等待队列，然后阻塞自己 */
		list_append(&psem->waiters, &running_thread()->general_tag);
		thread_block(TASK_BLOCKED);
	}
/* 若value为1或被唤醒后，会执行下面的代码，也就是获得了锁*/
	psem->value--;
	ASSERT(psem->value == 0);
	intr_set_status(old_status);
}

/* 信号量的up操作 */
void sema_up(struct semaphore* psem)
{
	/* 关中断 */
	enum intr_status old_status = intr_disable();
	ASSERT(psem->value == 0);
	if (!list_empty(&psem->waiters)) {
		struct task* thread_blocked = elem2entry(struct task, general_tag, list_pop(&psem->waiters));
		thread_unblock(thread_blocked);
	}
	psem->value++;
	ASSERT(psem->value == 1);
	intr_set_status(old_status);
}

/* 获取锁plock */
void lock_acquire(struct lock* plock)
{
/* 排除曾经自己已经持有锁但还未将其释放的情况 */
	if (plock->holder != running_thread()) {
		sema_down(&plock->semaphore);			//对信号量 P 操作，原子操作 
		plock->holder = running_thread();
		ASSERT(plock->holder_repeat_nr == 0);
		plock->holder_repeat_nr = 1;
	} else {
		plock->holder_repeat_nr++;
	}
}

/* 释放锁plock */
void lock_release(struct lock* plock)
{
	ASSERT(plock->holder == running_thread());
	if (plock->holder_repeat_nr > 1) {
		plock->holder_repeat_nr--;
		return;
	}
	ASSERT(plock->holder_repeat_nr == 1);

	plock->holder = NULL;			//把锁的持有者置空放在V操作之前
	plock->holder_repeat_nr = 0;
	sema_up(&plock->semaphore);		//信号量的 V 操作，也是原子操作
}
