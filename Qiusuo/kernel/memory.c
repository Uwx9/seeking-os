#include "memory.h"
#include "stdint.h"
#include "print.h"
#include "bitmap.h"
#include "debug.h"
#include "global.h"
#include "string.h"
#define PG_SIZE 4096

/************************  位图地址 ***************************** 
 * 因为0xc009f000是内核主线程栈顶，0xc009e000是内核主线程的pcb。
 * 一个页框大小的位图可表示128MB内存，位图位置安排在地址0xc009a000，
 * 这样本系统最大支持4个页框的位图，即512MB */
#define MEM_BITMAP_BASE 0xc0009a00

/* 0xc0000000 是内核从虚拟地址3G起。 
 * 0x100000 意指跨过低端1MB内存，使虚拟地址在逻辑上连续 */ 
#define K_HEAP_START 0xc0100000

#define PDE_IDX(addr) ((addr & 0xffc00000) >> 22)
#define PTE_IDX(addr) ((addr & 0x003ff000) >> 12)

/* 内存池结构，生成两个实例用于管理内核内存池和用户内存池 */ 
struct pool {
    struct bitmap pool_bitmap;      //本内存池用到的位图结构，用于管理物理内存
    uint32_t phy_addr_start;        //本内存池所管理物理内存的起始地址 
    uint32_t pool_size;             //本内存池字节容量 
};

struct pool kernel_pool, user_pool; // 生成内核内存池和用户内存池
struct virtual_addr_pool kernel_vaddr_pool;   // 此结构用来给内核分配虚拟地址 

/* 在pf表示的虚拟内存池中申请pg_cnt个虚拟页，
 * 成功则返回虚拟页的起始地址，失败则返回NULL */
static void* vaddr_get(enum pool_flags pf, uint32_t pg_cnt)
{   int bit_idx_start = -1;
    uint32_t cnt = 0, vaddr_start = 0; 
    if (pf == PF_KERNEL) {
        bit_idx_start = bitmap_scan(&kernel_vaddr_pool.vaddr_bitmap, pg_cnt);
        if (bit_idx_start == -1) {
            return NULL;
        } 
        /* 将刚申请的位图置1 */
        while (cnt < pg_cnt) {
            bitmap_set(&kernel_vaddr_pool.vaddr_bitmap, bit_idx_start + cnt++, 1);
        }
        vaddr_start = kernel_vaddr_pool.vaddr_start + bit_idx_start * PG_SIZE;
    } else {
        /* pf == PF_USER 的情况 */
    }
    return (void*)vaddr_start;
}    

/* 得到虚拟地址vaddr对应的pte指针(也是虚拟地址) */
static uint32_t* pte_ptr(uint32_t vaddr) 
{
    /* 先用高10位访问页目录，再用vaddr的高10位(页目录项的下标)在页目录中找到对应的页表
     * 再用vaddr的中间10位（页表项的下标）在页表中找到vaddr对应的页表项（在页表的4K里找到对应页表项，要用页表项下标*4）*/
    uint32_t* pte = (uint32_t*)(0xffc00000 + ((vaddr & 0xffc00000) >> 10) + PTE_IDX(vaddr) * 4);
    return pte;
}

/* 得到虚拟地址vaddr对应的pde的指针(也是虚拟地址) */
static uint32_t* pde_ptr(uint32_t vaddr)
{
    /* 虚拟地址的高10位是页目录项的下标 */
    uint32_t* pde = (uint32_t*)(0xfffff000 + PDE_IDX(vaddr) * 4);   
    return pde;
}

/* 在m_pool指向的物理内存池中分配1个物理页，成功则返回页框的物理地址，失败则返回NULL */
static void* palloc(struct pool* m_pool)
{
    /* 扫描或设置位图要保证原子操作 */
    int bit_idx = bitmap_scan(&(m_pool->pool_bitmap), 1);
    if (bit_idx == -1) {
        return NULL;
    }
    bitmap_set(&(m_pool->pool_bitmap), bit_idx, 1);
    uint32_t page_phyaddr = m_pool->phy_addr_start + bit_idx * PG_SIZE;
    return (void*)page_phyaddr;
}

/* 页表中添加虚拟地址_vaddr与物理地址_page_phyaddr的映射 */ 
static void page_table_add(void* _vaddr, void* _page_phyaddr)
{
    uint32_t vaddr = (uint32_t)_vaddr, page_phyaddr = (uint32_t)_page_phyaddr;
    uint32_t* pde = pde_ptr(vaddr);
    uint32_t* pte = pte_ptr(vaddr);

/************************   注意   ************************* 
* 执行*pte，会访问到空的pde,可能还没页表呢。所以确保pde创建完成后才能执行*pte
***********************************************************/ 

/* 先在页目录内判断目录项的P位，若为1，则表示该表已存在 */     
    if (*pde & 0x00000001) {     //页目录项和页表项的第0位为P，此处判断目录项是否存在
        /* 既然调用这个函数做映射，就说明pte处还没有写入物理页下标 */
        ASSERT(!(*pte & 0x00000001));
        if (!(*pte & 0x00000001)) {
            /* 将页表项与物理页映射 */
            *pte = page_phyaddr | PG_US_U | PG_RW_W | PG_P_1;
        } else {
            PANIC("pte_repeat");
            *pte = page_phyaddr | PG_US_U | PG_RW_W | PG_P_1;
        }
    } else {    // 页目录项不存在，所以要先创建页目录再创建页表项
        /* 页表中用到的页框一律从内核空间分配 */
        uint32_t pt_phyaddr = palloc(&kernel_pool);
        *pde = pt_phyaddr | PG_US_U | PG_RW_W | PG_P_1;

        /* 把刚申请的的页表清空 */
        memset((void*)((int)pte & 0xfffff000), 0, PG_SIZE);
        
        ASSERT(!(*pte & 0x00000001));
        *pte = page_phyaddr | PG_US_U | PG_RW_W | PG_P_1;

    }
}

/* 分配pg_cnt个页空间，成功则返回起始虚拟地址，失败时返回NULL */
void* malloc_page(enum pool_flags pf, uint32_t pg_cnt)
{
    ASSERT(pg_cnt > 0 && pg_cnt < 3840);    //3840页，15M

/***********   malloc_page 的原理是三个动作的合成:   ***********
1 通过vaddr_get在虚拟内存池中申请虚拟地址
2 通过palloc在物理内存池中申请物理页    
3 通过page_table_add将以上得到的虚拟地址和物理地址在页表中完成映射
********************************************************************/ 
    void* vaddr_start = vaddr_get(pf, pg_cnt);
    if (vaddr_start == NULL) {
        return NULL;
    }
    uint32_t vaddr = vaddr_start, cnt = pg_cnt;
    struct pool* mem_pool = pf & PF_KERNEL ? &kernel_pool : &user_pool;

    /* 因为虚拟地址是连续的，但物理地址可以是不连续的，所以逐个做映射*/
    while (cnt--) {
        void* page_phyaddr = palloc(mem_pool);
        if (!page_phyaddr) {    //失败时要将曾经已申请的虚拟地址和物理页全部回滚，在将来完成内存回收时再补充
            return NULL;
        }
        page_table_add((void*)vaddr, page_phyaddr);      //在页表中做映射
        vaddr += PG_SIZE;       //下一个虚拟页                       
    }
    return vaddr_start;
}

/* 从内核物理内存池中申请pg_cnt页内存，成功则返回其虚拟地址，失败则返回NULL */
void* get_kernel_pages(uint32_t pg_cnt) 
{
    void* vaddr = malloc_page(PF_KERNEL, pg_cnt);
    if (vaddr != NULL) {
        memset(vaddr, 0, pg_cnt * PG_SIZE);
    }
    return vaddr;
}

/* 初始化内存池 */
static void mem_pool_init(uint32_t all_mem)
{
    put_str("    mem_pool_init start\n");
/* 页表大小 = 1页的页目录表 + 第0和第768个页目录项指向同一个页表 + 
 * 第 769～1022个页目录项共指向254个页表，共256个页框 */
    uint32_t page_table_size = PG_SIZE * 256;  
    uint32_t used_mem = page_table_size + 0x100000;      //已经使用的内存为256个页框和低端1M
    uint32_t free_mem = all_mem - used_mem;
/* 对于以页为单位的内存分配策略,不足1页的内存不用考虑了 
 * 如果是4G，最多就1023个可用页表(还有1个页表的4M是一个页目录和1023个页表)
 * 这里32M最多8K个页，16位够用了 */
    uint16_t all_free_pages = free_mem / PG_SIZE;        
    uint16_t kernel_free_pages = all_free_pages / 2;
    uint16_t user_free_pages = all_free_pages - kernel_free_pages;

/* 为简化位图操作，余数不处理，坏处是这样做会丢内存。
 * 好处是不用做内存的越界检查，因为位图表示的内存少于实际物理内存 */
    uint16_t kbm_lenth = kernel_free_pages / 8;         //Kernel BitMap 的长度
    uint16_t ubm_lenth = user_free_pages / 8;           //User BitMap 的长度

    uint32_t kp_start = used_mem;       //Kernel Pool start，内核内存池的起始地址
    uint32_t up_start = kp_start + kernel_free_pages * PG_SIZE;     //User Pool start，用户内存池的起始地址

    kernel_pool.phy_addr_start = kp_start;
    kernel_pool.pool_size = kernel_free_pages * PG_SIZE;
    kernel_pool.pool_bitmap.btmp_bytes_len = kbm_lenth;

    user_pool.phy_addr_start = up_start;
    user_pool.pool_size = user_free_pages * PG_SIZE;
    user_pool.pool_bitmap.btmp_bytes_len = ubm_lenth;

/*********    内核内存池和用户内存池位图   ***********    
 * 位图是全局的数据，长度不固定。
 * 全局或静态的数组需要在编译时知道其长度，
 * 而我们需要根据总内存大小算出需要多少字节，
 * 所以改为指定一块内存来生成位图。
 * ************************************************/
// 内核使用的最高地址是0xc009f000，这是主线程的栈地址 
// （内核的大小预计为70KB左右） 
// 32MB 内存占用的位图是1KB 
// 内核内存池的位图先定在MEM_BITMAP_BASE(0xc009a000)处
    kernel_pool.pool_bitmap.bits = (void*)MEM_BITMAP_BASE;
    user_pool.pool_bitmap.bits = (void*)(MEM_BITMAP_BASE + kbm_lenth);
    
/********************输出内存池信息**********************/
    put_str("    kernel_pool_bitmap_start:");
    put_int((int)kernel_pool.pool_bitmap.bits);
    put_str("    kernel_pool_phy_addr_start:");
    put_int((int)kernel_pool.phy_addr_start);
    put_char('\n');

    put_str("    user_pool_bitmap_start:");
    put_int((int)user_pool.pool_bitmap.bits);
    put_str("    user_pool_phy_addr_start:");
    put_int((int)user_pool.phy_addr_start);
    put_char('\n');

    /* 将位图置0 */
    bitmap_init(&kernel_pool.pool_bitmap);
    bitmap_init(&user_pool.pool_bitmap);

    /* 下面初始化内核虚拟地址的位图，按实际物理内存大小生成数组。
     * 用于维护内核堆的虚拟地址，所以要和内核内存池大小一致 */
    kernel_vaddr_pool.vaddr_bitmap.btmp_bytes_len = kbm_lenth;
    /* 位图的数组指向一块未使用的内存,目前定位在内核内存池和用户内存池之外 */
    kernel_vaddr_pool.vaddr_bitmap.bits = (void*)(MEM_BITMAP_BASE + kbm_lenth + ubm_lenth);

    /* 虚拟地址内存池只有个起始地址和位图 */
    kernel_vaddr_pool.vaddr_start = K_HEAP_START;
    bitmap_init(&kernel_vaddr_pool.vaddr_bitmap);
    put_str("    mem_pool_init: done\n");
}

/* 内存管理部分初始化入口 */
void mem_init()
{
    put_str("    mem_init start...\n");
    uint32_t mem_bytes_total = *((uint32_t*)0xb00); //值为0，可能前面获取内存有bug
    put_str("all mem is:");
    put_int(mem_bytes_total);
    mem_pool_init(mem_bytes_total);
    put_str("mem_init done\n");
}