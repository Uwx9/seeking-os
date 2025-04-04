#ifndef __DEVICE_IOQUEUE_H
#define __DEVICE_IOQUEUE_H
#include "stdint.h"
#include "thread.h"
#include "sync.h"
#include "global.h"

#define bufsize 64

struct ioqueue {
	struct lock lock;
/* 生产者，缓冲区不满时就继续往里面放数据,* 否则就睡眠，此项记录哪个生产者在此缓冲区上睡眠 */
	struct task* producer;
/* 消费者，缓冲区不空时就继续从里面拿数据,否则就睡眠，此项记录哪个消费者在此缓冲区上睡眠 */	
	struct task* consumer;
	
	char buf[bufsize];
	uint32_t head;
	uint32_t tail;
};

void ioqueue_init(struct ioqueue* ioq);
bool ioq_full(struct ioqueue* ioq);
bool ioq_empty(struct ioqueue* ioq);
char ioq_getchar(struct ioqueue* ioq);
void ioq_putchar(struct ioqueue* ioq, char byte);
#endif
