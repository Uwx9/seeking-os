#include "inode.h"
#include "stdint.h"
#include "debug.h"
#include "ide.h"
#include "super_block.h"
#include "fs.h"
#include "global.h"
#include "string.h"
#include "thread.h"
#include "interrupt.h"
#include "file.h"

/* 用来存储inode位置 */
struct inode_position {
	bool two_sec;		// inode 是否跨扇区 
	uint32_t sec_lba;	// inode 所在的扇区号
	uint32_t off_size;	// inode 在扇区内的字节偏移量
};

/* 获取inode所在的扇区和扇区内的偏移量 */
static void inode_locate(struct partition* part, uint32_t inode_no, struct inode_position* inode_pos)
{
	ASSERT(inode_no < 4096);
	uint32_t inode_table_lba = part->sb->inode_table_lba;
	uint32_t inode_size = sizeof(struct inode);
	// 偏移字节数, 是inode_no这个i节点前面偏移的字节数, 所以要判断这个inode是否跨扇区
	uint32_t off_size = inode_no * inode_size;
	// 偏移扇区数，相对inode起始扇区
	uint32_t off_sec = off_size / SECTOR_SIZE;
	// 在最后一个扇区的偏移量
	uint32_t off_size_in_sec = off_size % SECTOR_SIZE;
	// 若剩余的字节数小于一个inode的大小
	if (SECTOR_SIZE - off_size_in_sec < inode_size) {
		inode_pos->two_sec = true;
	} else {
		inode_pos->two_sec = false;
	}
	inode_pos->sec_lba = inode_table_lba + off_sec;
	inode_pos->off_size = off_size;
}

/* 将 inode 写入到分区part,需要提供1024字节的io_buf */
void inode_sync(struct partition* part, struct inode* inode, void* io_buf)	// io_buf是用于硬盘io的缓冲区
{
	uint32_t inode_no = inode->i_no;
	struct inode_position inode_pos;
	// 将inode位置信息写入inode_pos
	inode_locate(part, inode_no, &inode_pos);
	ASSERT(inode_pos.sec_lba <= (part->start_lba + part->sec_cnt));
	// 硬盘中的inode中的成员inode_tag和i_open_cnts是不需要的,它们只在内存中记录链表位置和被多少进程共享 
	struct inode pure_inode;
	memcpy(&pure_inode, inode, sizeof(struct inode));
	// 以下 inode的三个成员只存在于内存中,现在将inode同步到硬盘，清掉这三项即可
	pure_inode.i_open_cnts = 0;
	pure_inode.inode_tag.next = pure_inode.inode_tag.prev = NULL;
	pure_inode.writen_deny = false;

	char* inode_buf = (char*)io_buf;
	if (inode_pos.two_sec) {
		// 若是跨了两个扇区，就要读出两个扇区再写入两个扇区
		// 读写硬盘是以扇区为单位，若写入的数据小于一扇区，要将原硬盘上的内容先读出来再和新数据拼成一扇区后再写入 
		ide_read(part->mydisk, inode_pos.sec_lba, inode_buf, 2);
		memcpy(inode_buf + inode_pos.off_size, &pure_inode, sizeof(struct inode));
		ide_write(part->mydisk, inode_pos.sec_lba, inode_buf, 2);
	} else {
		ide_read(part->mydisk, inode_pos.sec_lba, inode_buf, 1);
		memcpy(inode_buf + inode_pos.off_size, &pure_inode, sizeof(struct inode));
		ide_write(part->mydisk, inode_pos.sec_lba, inode_buf, 1);
	}
}

/* 根据 i结点号返回相应的i结点 */ 
struct inode* inode_open(struct partition* part, uint32_t inode_no)
{
	// 去这个分区打开的inode中找
	struct list_elem* elem = part->open_inodes.head.next;
	struct inode* retinode;
	while (elem != &part->open_inodes.tail) {
		retinode = elem2entry(struct inode, inode_tag, elem);
		if (retinode->i_no == inode_no) {
			retinode->i_open_cnts++;
			return retinode;
		}
		elem = elem->next;
	}
	// 若没有在分区的open_inodes中找到inode则在硬盘上找
	struct inode_position inode_pos;
	inode_locate(part, inode_no, &inode_pos);
	// 在内核空间中为inode申请空间,所以临时将pgdir置为NULL,sys_malloc判断时会从内核申请
	struct task* cur = running_thread();
	uint32_t* cur_pgdir = cur->pgdir;
	cur->pgdir = NULL;
	retinode = (struct inode*)sys_malloc(sizeof(struct inode));
	// 恢复pgdir
	cur->pgdir = cur_pgdir;
	// 将inode所在的扇区读出来,考虑是否跨扇区
	char* inode_buf;
	if (inode_pos.two_sec) {
		inode_buf = sys_malloc(SECTOR_SIZE * 2);
		ide_read(part->mydisk, inode_pos.sec_lba, inode_buf, 2);
	} else {
		inode_buf = sys_malloc(SECTOR_SIZE * 1);
		ide_read(part->mydisk, inode_pos.sec_lba, inode_buf, 1);
	}
	memcpy(retinode, inode_buf + inode_pos.off_size, sizeof(struct inode));
	// 因为一会很可能要用到此inode，故将其插入到队首便于提前检索到
	list_push(&part->open_inodes, &retinode->inode_tag);
	retinode->i_open_cnts = 1;

	sys_free(inode_buf);
	return retinode;
}

/* 关闭inode或减少inode的打开数 */
void inode_close(struct inode* inode)
{
	enum intr_status old_status = intr_disable();
	if (--inode->i_open_cnts == 0) {
		list_remove(&inode->inode_tag);
		// 释放 inode时也要确保释放的是内核内存池
		struct task* cur = running_thread();
		uint32_t* cur_pgdir = cur->pgdir;
		cur->pgdir = NULL;
		sys_free(inode);
		cur->pgdir = cur_pgdir;
	}
	intr_set_status(old_status);
}

/* 将硬盘分区part上的inode_no号inode清空 */
void inode_delete(struct partition* part, uint32_t inode_no, void* io_buf)
{
	ASSERT(inode_no < 4096);
	struct inode_position inode_pos;
	
	// 将inode位置信息写入inode_pos
	inode_locate(part, inode_no, &inode_pos);
	ASSERT(inode_pos.sec_lba <= (part->start_lba + part->sec_cnt));

	char* inode_buf = (char*)io_buf;
	if (inode_pos.two_sec == false) {
		/* 将原硬盘上的内容先读出来 */
		ide_read(part->mydisk, inode_pos.sec_lba, inode_buf, 1);
		/* 将 inode_buf 清 0 */
		memset((inode_buf + inode_pos.off_size), 0, sizeof(struct inode));
		/* 用清 0 的内存数据覆盖磁盘 */
		ide_write(part->mydisk, inode_pos.sec_lba, inode_buf, 1);
	} else {	// inode跨扇区,读两个扇区
		/* 将原硬盘上的内容先读出来 */
		ide_read(part->mydisk, inode_pos.sec_lba, inode_buf, 2);
		/* 将 inode_buf 清 0 */
		memset((inode_buf + inode_pos.off_size), 0, sizeof(struct inode));
		/* 用清 0 的内存数据覆盖磁盘 */
		ide_write(part->mydisk, inode_pos.sec_lba, inode_buf, 2);
	}
}

/* 回收 inode的数据块和inode本身 */
void inode_realse(struct partition* part, uint32_t inode_no)
{
	struct inode* inode2del = inode_open(part, inode_no);
	ASSERT(inode2del->i_no == inode_no);

	/* 1 回收 inode占用的所有块 */
	uint8_t block_idx = 0, block_cnt = 12;
	uint32_t block_bitmap_idx;
	uint32_t all_blocks[140] = {0};		// 140个数据块

	/* a 先将前12个直接块存入all_blocks */
	while (block_idx < 12) {
		all_blocks[block_idx] = inode2del->i_sectors[block_idx];
		block_idx++;
	}
	
	/* b 如果一级间接块表存在，将其128个间接块读到all_blocks[12~]，并释放一级间接块表所占的扇区 */
	if (inode2del->i_sectors[12] != 0) {
		ide_read(part->mydisk, inode2del->i_sectors[12], all_blocks + 12, 1);
		block_cnt = 140;

		block_bitmap_idx = inode2del->i_sectors[12] - part->sb->data_start_lba;
		ASSERT(block_bitmap_idx > 0);
		bitmap_set(&part->block_bitmap, block_bitmap_idx, 0);
		// 
		bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
	}

	/* c inode 所有的块地址已经收集到all_blocks中，下面逐个回收 */
	block_idx = 0;
	while (block_idx < block_cnt) {
		if (all_blocks[block_idx] != 0) {
			block_bitmap_idx = 0;
			block_bitmap_idx = all_blocks[block_idx] - part->sb->data_start_lba;
			ASSERT(block_bitmap_idx > 0);
			bitmap_set(&part->block_bitmap, block_bitmap_idx, 0);
			bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
		}
		block_idx++;
	}
	
	/*2 回收该inode所占用的inode */
	bitmap_set(&part->inode_bitmap, inode_no, 0);
	bitmap_sync(cur_part, inode_no, INODE_BITMAP);

	/******     以下inode_delete是调试用的    ****** 
	* 此函数会在inode_table中将此inode清0， 
	* 但实际上是不需要的，inode分配是由inode位图控制的， 
	* 硬盘上的数据不需要清0，可以直接覆盖 */	
	void* io_buf = sys_malloc(1024);
	inode_delete(part, inode_no, io_buf);
	sys_free(io_buf);

	// 上面的操作是删除硬盘里的inode及回收数据块,下面还要删除在内存中的打开的inode
	inode_close(inode2del);
}

/* 初始化new_inode */
void inode_init(uint32_t inode_no, struct inode* new_inode)
{
	new_inode->i_no = inode_no;
	new_inode->i_open_cnts = 0;
	new_inode->i_size = 0;
	new_inode->writen_deny = false;
	//  初始化块索引数组i_sector
	uint8_t sec_idx = 0;
	while(sec_idx < 13) {
		// 直接块和一级间接块指针设为0
		new_inode->i_sectors[sec_idx] = 0;
		sec_idx++;
	}

}