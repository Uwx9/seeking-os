#ifndef __LIB_KERNEL_BITMAP_H
#define __LIB_KERNEL_BITMAP_H
#include "global.h"
#define BITMAP_MASK 1

struct bitmap {
    uint32_t btmp_bytes_len;
/* 在遍历位图时，整体上以字节为单位，细节上是以位为单位， 
 * 所以此处位图的指针必须是单字节
 * 位图长度取决于所管理资源的大小，其长度不固定，因此不能在位图结构struct bitmap中生成
 * 固定大小的位图数组。因此一种“乐观”的解决方案是在struct bitmap中提供位图的指针 */
    uint8_t* bits;
};

void bitmap_init(struct bitmap* btmp);
bool bitmap_scan_test(struct bitmap* btmp, uint32_t bit_idx);
int bitmap_scan(struct bitmap* btmp, uint32_t cnt);
void bitmap_set(struct bitmap* btmp, uint32_t bit_idx, uint8_t value);

#endif