#include "memory.h"
#include "stdint.h"
#include "print.h"
#include "bitmap.h"
#include "debug.h"
#include "global.h"
#include "string.h"
#include "sync.h"
#include "list.h"
#include "interrupt.h"
#include "debug.h"

#define PG_SIZE 4096

/************************  位图地址 ***************************** 
 * 因为0xc009f000是内核主线程栈顶，0xc009e000是内核主线程的pcb。
 * 一个页框大小的位图可表示128MB内存，位图位置安排在地址0xc009a000，
 * 这样本系统最大支持4个页框的位图，即512MB */
#define MEM_BITMAP_BASE 0xc009a000

/* 0xc0000000 是内核从虚拟地址3G起。 
 * 0x100000 意指跨过低端1MB内存，使虚拟地址在逻辑上连续 */ 
#define K_HEAP_START 0xc0100000

#define PDE_IDX(addr) ((addr & 0xffc00000) >> 22)
#define PTE_IDX(addr) ((addr & 0x003ff000) >> 12)

/* 内存池结构，生成两个实例用于管理内核内存池和用户内存池 */ 
struct pool {
    struct bitmap pool_bitmap;      // 本内存池用到的位图结构，用于管理物理内存
    uint32_t phy_addr_start;        // 本内存池所管理物理内存的起始地址 
    uint32_t pool_size;             // 本内存池字节容量 
	struct lock lock;				// 申请内存时互斥
};

struct pool kernel_pool, user_pool; // 生成内核内存池和用户内存池
struct virtual_addr_pool kernel_vaddr_pool;   // 此结构用来给内核分配虚拟地址 

/* 内存仓库 */
struct arena {
	struct mem_block_desc* desc;	// 该arena关联的mem_block_desc
	uint32_t cnt;					// large 为 ture 时，cnt表示的是页框数,否则cnt表示空闲mem_block数量 
	bool large;
};

struct mem_block_desc k_block_descs[DESC_CNT];

// 为malloc做准备
void block_descs_init(struct mem_block_desc* desc_arrary)
{
	uint32_t i, size = 16;
	for (i = 0; i < DESC_CNT; i++) {
		desc_arrary[i].block_size = size;
		desc_arrary[i].blocks_per_arena = (PG_SIZE - sizeof(struct arena)) / size;
		list_init(&desc_arrary[i].free_list);
		size *= 2;
	}
}

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
		struct task* cur = running_thread();
		bit_idx_start = bitmap_scan(&cur->userprog_vaddr.vaddr_bitmap, pg_cnt);
		if (bit_idx_start == -1) {
			return NULL;
		}
		while (cnt < pg_cnt) {
			bitmap_set(&cur->userprog_vaddr.vaddr_bitmap, bit_idx_start + cnt++, 1);	
		}
		vaddr_start = cur->userprog_vaddr.vaddr_start + bit_idx_start * PG_SIZE;
		
		/* (0xc0000000 - PG_SIZE)作为用户3级栈已经在start_process被分配 */
		ASSERT((uint32_t)vaddr_start < (0xc0000000 - PG_SIZE));
    }
    return (void*)vaddr_start;
}    

/* 得到虚拟地址vaddr对应的pte指针(也是虚拟地址) */
uint32_t* pte_ptr(uint32_t vaddr) 
{
    /* 先用高10位访问页目录，再用vaddr的高10位(页目录项的下标)在页目录中找到对应的页表
     * 再用vaddr的中间10位（页表项的下标）在页表中找到vaddr对应的页表项（在页表的4K里找到对应页表项，要用页表项下标*4）*/
    uint32_t* pte = (uint32_t*)(0xffc00000 + ((vaddr & 0xffc00000) >> 10) + PTE_IDX(vaddr) * 4);
    return pte;
}

/* 得到虚拟地址vaddr对应的pde的指针(也是虚拟地址) */
uint32_t* pde_ptr(uint32_t vaddr)
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
        uint32_t pt_phyaddr = (uint32_t)palloc(&kernel_pool);
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
    uint32_t vaddr = (uint32_t)vaddr_start, cnt = pg_cnt;
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

/* 在用户空间中申请4k内存，并返回其虚拟地址 */
void* get_user_pages(uint32_t pg_cnt)
{
	lock_acquire(&user_pool.lock);
	void* vaddr = malloc_page(PF_USER, pg_cnt);
	if (vaddr) {
		memset(vaddr, 0, pg_cnt * PG_SIZE);
	}
	lock_release(&user_pool.lock);
	return vaddr;
}

/* 将地址vaddr与pf池中的物理地址关联，仅支持一页空间分配, 并返回虚拟地址 */ 
void* get_a_page(enum pool_flags pf, uint32_t vaddr)
{
	struct pool* mem_pool = pf & PF_KERNEL ? &kernel_pool : &user_pool;
	lock_acquire(&mem_pool->lock);

	int bit_idx = -1;
	struct task* cur = running_thread();

/* 若当前是用户进程申请用户内存，就修改用户进程自己的虚拟地址位图 */
	if (cur->pgdir != NULL && pf == PF_USER) {
		bit_idx = (vaddr - cur->userprog_vaddr.vaddr_start) / PG_SIZE;
		ASSERT(bit_idx >= 0);
		bitmap_set(&cur->userprog_vaddr.vaddr_bitmap, bit_idx, 1);
	} else if (cur->pgdir == NULL && pf == PF_KERNEL) {
/* 如果是内核线程申请内核内存，就修改kernel_vaddr */
		bit_idx = (vaddr - kernel_vaddr_pool.vaddr_start) / PG_SIZE;
		ASSERT(bit_idx > 0);
		bitmap_set(&kernel_vaddr_pool.vaddr_bitmap, bit_idx, 1);
	} else {
		PANIC("get_a_page: not allow kernel alloc userspace or user alloc kernelspace by get_a_page ");
	}
	
	void* page_phyaddr = palloc(mem_pool);
	if (page_phyaddr == NULL) {
		lock_release(&mem_pool->lock);
		return NULL;
	}
	page_table_add((void*)vaddr, page_phyaddr);
	lock_release(&mem_pool->lock);
	return (void*)vaddr;
}

/* 安装1页大小的vaddr，专门针对fork时虚拟地址位图无需操作的情况 */
void* get_a_page_without_opvaddrbitmap(enum pool_flags pf, uint32_t vaddr)
{
	struct pool* mem_pool = pf & PF_KERNEL ? &kernel_pool : &user_pool;
	lock_acquire(&mem_pool->lock);
	void* page_phyaddr = palloc(mem_pool);
	if (page_phyaddr == NULL) {
		lock_release(&mem_pool->lock);
		return NULL;
	}
	page_table_add((void*)vaddr, page_phyaddr);
	lock_release(&mem_pool->lock);
	return (void*)vaddr;
}


/* 得到虚拟地址映射到的物理地址 */ 
uint32_t addr_v2p(uint32_t vaddr)
{
	uint32_t* pte = pte_ptr(vaddr);
	/* (*pte)&0xfffff000的值是页表所在的物理页框起始地址 */
	return (uint32_t)(*pte & 0xfffff000) + (vaddr & 0x00000fff);
}

/* 初始化内存池 */
static void mem_pool_init(uint32_t all_mem)
{
    put_str("    mem_pool_init start\n", 0x07);

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
    put_str("    kernel_pool_bitmap_start:", 0x07);
    put_int((int)kernel_pool.pool_bitmap.bits);
    put_str("    kernel_pool_phy_addr_start:", 0x07);
    put_int((int)kernel_pool.phy_addr_start);
    put_char('\n', 0x07);

    put_str("    user_pool_bitmap_start:", 0x07);
    put_int((int)user_pool.pool_bitmap.bits);
    put_str("    user_pool_phy_addr_start:", 0x07);
    put_int((int)user_pool.phy_addr_start);
    put_char('\n', 0x07);

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
	lock_init(&kernel_pool.lock);
	lock_init(&user_pool.lock);
    put_str("    mem_pool_init: done\n", 0x07);
}


/* ****************************************************************************  */ 
/*                          实现sys_malloc                                       */ 
/* **************************************************************************** */ 
/* 返回arena中第idx个内存块的地址 */ 
static struct mem_block* arena2block(struct arena* a, uint32_t idx)
{
	// 也就是说每个小内存块开头都是一个mem_block
	return (struct mem_block*)((uint32_t)a + sizeof(struct arena) + idx * a->desc->block_size);
}

/* 返回内存块b所在的arena地址 */
static struct arena* block2arena(struct mem_block* b)
{
	return (struct arena*)((uint32_t)b & 0xfffff000);
}

/* 在堆中申请size字节内存 */
void* sys_malloc(uint32_t size)
{
	enum pool_flags PF;				// 内存池类型
	struct pool* pl;				// 内存池的指针
	uint32_t pool_size;				// 所用内存池的大小
	struct mem_block_desc* desc;	// 所用的内存块描述符，由size决定	
	struct task* cur_thread = running_thread();	// 当前线程或进程

	if (cur_thread->pgdir != NULL) {	// 用户进程申请
		PF = PF_USER;
		pl = &user_pool;
		pool_size = user_pool.pool_size;
		desc = cur_thread->u_block_descs;
	} else {							// 内核申请
		PF = PF_KERNEL;
		pl = &kernel_pool;
		pool_size = kernel_pool.pool_size;
		desc = k_block_descs;
	}

	if (!(size > 0 && size < pool_size)) {
		return NULL;
	}
	struct arena* a;
	struct mem_block* b;
	lock_acquire(&pl->lock);

	if (size > 1024) {
		uint32_t pg_cnt = DIV_ROUND_UP(size + sizeof(struct arena), PG_SIZE);
		a = malloc_page(PF, pg_cnt);
		// 申请成功，将申请到的空间清0
		if (a != NULL) {
			/* 对于分配的大块页框，将desc置为NULL,cnt 置为页框数，large置为true */
			memset(a, 0, pg_cnt * PG_SIZE);
			a->cnt = pg_cnt;
			a->desc = NULL;
			a->large = true;
			lock_release(&pl->lock);
			return (void*)(a + 1);
		} else {
			lock_release(&pl->lock);
			return NULL;
		}
	} else {		// 若申请的内存小于等于1024,可在各种规格的mem_block_desc中去适配 
		uint8_t desc_idx;	// 要用的内存块描述符下标
		
		/* 从内存块描述符中匹配合适的内存块规格 */ 
		for (desc_idx = 0; desc_idx < DESC_CNT; desc_idx++) {
			if (size <= desc[desc_idx].block_size) {
				break;
			}
		}
		/* 若 mem_block_desc 的 free_list 中已经没有可用的mem_block，就创建新的arena提供mem_block */
		if (list_empty(&desc[desc_idx].free_list)) {
			a = malloc_page(PF, 1);
			if (a == NULL) {
				lock_release(&pl->lock);
				return NULL;
			}
			memset(a, 0, PG_SIZE);
			/* 对于分配的小块内存，将desc置为相应内存块描述符，cnt 置为此arena可用的内存块数,large置为false */
			a->cnt = desc[desc_idx].blocks_per_arena;
			a->desc = &desc[desc_idx];
			a->large = false;
			
			uint32_t block_idx;
			enum intr_status old_status = intr_disable();	// 对arena操作时不应该调度

			/* 开始将arena拆分成内存块，并添加到内存块描述符的free_list中 */
			for (block_idx = 0; block_idx < a->desc->blocks_per_arena; block_idx++) {
				b = arena2block(a, block_idx);
				ASSERT(!elem_find(&a->desc->free_list, &b->free_elem));
				list_append(&a->desc->free_list, &b->free_elem);
			}
			intr_set_status(old_status);
		}
		/* 开始分配内存块 */
		// 这里差点用了a->desc->free_list,不能这样,因为a不会先转换为要用的a,是先早到要用的mem_block再切换arena
		b = elem2entry(struct mem_block, free_elem, list_pop(&(desc[desc_idx].free_list)));
		// 这里实际是把小内存块开头的mem_block也清空了吧,对于比较小的内存块这不至于造成较大的浪费
		memset(b, 0, desc[desc_idx].block_size);
		a = block2arena(b);
		a->cnt--;
		lock_release(&pl->lock);
		return (void*)b;
	}
}

/* ****************************************************************************  */ 
/*                          实现sys_free                                         */ 
/* 总结起来就3步, 1:释放物理页框, 2:修改vaddr对物理页的映射, 3:释放虚拟内存页           */
/* *************************************************************************** */ 

/* 将物理地址pg_phy_addr回收到物理内存池 */
static void pfree(uint32_t pg_phyaddr)
{
	struct pool* pl;
	uint32_t bit_idx;
	if (pg_phyaddr >= user_pool.phy_addr_start) {
		pl = &user_pool;
		bit_idx = (pg_phyaddr - pl->phy_addr_start) / PG_SIZE;
	} else {
		pl = &kernel_pool;
		bit_idx = (pg_phyaddr - pl->phy_addr_start) / PG_SIZE;
	}
	bitmap_set(&pl->pool_bitmap, bit_idx, 0);
}

/* 去掉页表中虚拟地址vaddr的映射，只去掉vaddr对应的pte */
static void page_table_pte_remove(uint32_t vaddr)
{
	uint32_t* pte = pte_ptr(vaddr);
	// 将页表项 pte 的P位置0 
	*pte &= ~PG_P_1;		// ~PG_P_1 = 0xFFFFFFFE
	asm volatile ("invlpg %0" : : "m"(vaddr) : "memory");
}

/* 在虚拟地址池中释放以_vaddr起始的连续pg_cnt个虚拟页地址 */
static void vaddr_remove(enum pool_flags PF, void* _vaddr, uint32_t pg_cnt)
{
	uint32_t bit_idx_start = 0, vaddr = (uint32_t)_vaddr, cnt = 0;
	if (PF == PF_KERNEL) {
		bit_idx_start = (vaddr - kernel_vaddr_pool.vaddr_start) / PG_SIZE;
		while (cnt < pg_cnt) {
			bitmap_set(&kernel_vaddr_pool.vaddr_bitmap, bit_idx_start + cnt++, 0);
		}
	} else {	// 用户虚拟内存池
		struct task* cur_thread = running_thread();
		bit_idx_start = (vaddr - cur_thread->userprog_vaddr.vaddr_start) / PG_SIZE;
		while (cnt < pg_cnt) {
			bitmap_set(&cur_thread->userprog_vaddr.vaddr_bitmap, bit_idx_start + cnt++, 0);
		}
	}
}

/* 释放以虚拟地址vaddr为起始的cnt个物理页框 */
void mfree_page(enum pool_flags pf, void* _vaddr, uint32_t pg_cnt)
{
	uint32_t pg_phyaddr, cnt = 0;;
	uint32_t vaddr = (uint32_t)_vaddr;
	ASSERT(((uint32_t)_vaddr % PG_SIZE == 0) && pg_cnt > 0);
	pg_phyaddr = addr_v2p(vaddr);

	ASSERT((pg_phyaddr % PG_SIZE == 0) && (pg_phyaddr >= 0x102000));
	if (pg_phyaddr >= user_pool.phy_addr_start) {
		vaddr -= PG_SIZE;
		while (cnt < pg_cnt) {
			// 我觉得这样写确实可能好点,把要用的参数在最开始准备好
			vaddr += PG_SIZE;
			pg_phyaddr = addr_v2p(vaddr);
			// 确保释放的物理内存属于用户内存池,每次都要检查
			ASSERT((pg_phyaddr % PG_SIZE) == 0 && pg_phyaddr >= user_pool.phy_addr_start);
			// 释放物理页框,主要是把物理内存池的位图对应的位置0
			pfree(pg_phyaddr);
			// 将对应的页目录项P位置0
			page_table_pte_remove(vaddr);
			cnt++;
		}
		// 一次性释放pg_cnt个虚拟页,把虚拟内存池的位图对应的位置0
		// 真正使映射失效的是page_table_pte_remove,这里是维护位图
		vaddr_remove(pf, _vaddr, pg_cnt);
	} else {
		vaddr -= PG_SIZE;
		while (cnt < pg_cnt) {
			vaddr += PG_SIZE;
			pg_phyaddr = addr_v2p(vaddr);
			// 确保释放的物理内存属于内核内存池,每次都要检查
			ASSERT((pg_phyaddr % PG_SIZE) == 0 && pg_phyaddr >= kernel_pool.phy_addr_start \
			&& pg_phyaddr < user_pool.phy_addr_start);
			// 释放物理页框
			pfree(pg_phyaddr);
			// 将对应的页目录项P位置0
			page_table_pte_remove(vaddr);
			cnt++;
		}
		// 一次性释放pg_cnt个虚拟页
		vaddr_remove(pf, _vaddr, pg_cnt);
	}
}

/* 根据物理页框地址pg_phy_addr在相应的内存池的位图清0，不改动页表*/
void free_a_phy_addr(uint32_t pg_phyaddr)	// 这个函数和前面的pfree一样的
{
	struct pool* mem_pool;
	uint32_t bit_idx = 0;
	if (pg_phyaddr >= user_pool.phy_addr_start) {
		mem_pool = &user_pool;
		bit_idx = (pg_phyaddr - user_pool.phy_addr_start) / PG_SIZE;
	} else {
		mem_pool = &kernel_pool;
		bit_idx = (pg_phyaddr - kernel_pool.phy_addr_start) / PG_SIZE;
	}
	bitmap_set(&mem_pool->pool_bitmap, bit_idx, 0);
}

/* 回收内存ptr */
void sys_free(void* ptr)
{
	ASSERT(ptr != NULL);
	if (ptr != NULL) {
		enum pool_flags pf;
		struct pool* pl;
		struct task* cur_thread = running_thread();
		// 得到当前线程或进程对应的pf和pl
		if (cur_thread->pgdir == NULL) {
			ASSERT((uint32_t)ptr >= K_HEAP_START);
			pf = PF_KERNEL;
			pl = &kernel_pool;
		} else {
			pf = PF_USER;
			pl = &user_pool;
		}

		lock_acquire(&pl->lock);
		struct mem_block* b = ptr;
		struct arena* a = block2arena(b);
		ASSERT(a->large == 1 || a->large == 0);
		if (a->desc == NULL && a->large == true) {	//回收大内存,>= 1024
			mfree_page(pf, a, a->cnt);
		} else {		// 回收小内存,<1024
			list_append(&a->desc->free_list, &b->free_elem);
			// 如果收回一块小内存,并且a->cnt是块数-1,就说明所有内存块空闲,就回收arena,如果不是也可以让cnt+1
			if (++a->cnt == a->desc->blocks_per_arena) {
				uint32_t bk_idx;
				for (bk_idx = 0; bk_idx < a->desc->blocks_per_arena; bk_idx++) {
					b = arena2block(a, bk_idx);
					ASSERT(elem_find(&a->desc->free_list, &b->free_elem))
					list_remove(&b->free_elem);
				}
				mfree_page(pf, a, 1);
			}
		}
		lock_release(&pl->lock);
	}
}

/* 内存管理部分初始化入口 */
void mem_init()
{
    put_str("    mem_init start...\n", 0x07);
    uint32_t mem_bytes_total = *((uint32_t*)0xb00); 
    put_str("all mem is:", 0x07);
    put_int(mem_bytes_total);
    mem_pool_init(mem_bytes_total);
	// 初始化内存块描述符
	block_descs_init(k_block_descs);
    put_str("mem_init done\n", 0x07);
}

