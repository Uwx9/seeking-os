#ifndef __USERPROG_PROCESS_H
#define __USERPROG_PROCESS_H
#include "stdint.h"
#include "thread.h"

#define USER_VADDR_START 0x8048000
#define default_prio 30

void start_process(void* filename_);
void page_dir_activate(struct task* pthread);
void process_activate(struct task* pthread);
uint32_t* create_page_dir(void);
void create_user_vaddr_bitmap(struct task* user_prog);
void process_execute(void* filename, char* name);
#endif