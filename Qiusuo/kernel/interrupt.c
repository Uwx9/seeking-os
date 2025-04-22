#include "interrupt.h"
#include "stdint.h"
#include "global.h"
#include "io.h"
#include "print.h"

#define PIC_M_CTRL 0x20     // 主片的控制端口是0x20
#define PIC_M_DATA 0x21     // 主片的数据端口是0x21
#define PIC_S_CTRL 0xa0     // 从片的控制端口是0xa0
#define PIC_S_DATA 0xa1     // 从片的数据端口是0xa1

#define IDT_DESC_CNT 0x81   //当前总的支持的中断数

#define EFLAGS_IF 0x00000200
#define GET_EFLAGS(EFLAGS_VAR) asm volatile ("pushfl; popl %0" : "=g"(EFLAGS_VAR))

extern uint32_t syscall_handler(void);

struct gate_desc {
    uint16_t func_offset_low_word;
    uint16_t selector;
    uint8_t dcount;          //此项为双字计数字段，是门描述符中的第4字节,此项固定值
    uint8_t attribute;
    uint16_t func_offset_high_word;
};

static void make_idt_desc(struct gate_desc* p_gdesc, uint8_t attr, intr_handler function);
static struct gate_desc idt[IDT_DESC_CNT];				//中断门描述符数组
char* intr_name[IDT_DESC_CNT];							//用于保存异常的名字

/* 定义中断处理程序数组，在kernel.S中定义的intrXXentry
 * 只是中断处理程序的入口(做了一些准备工作)，然后call idt_func_table中的元素
 * 最终调用的是idt_func_table中的处理程序(地址)，
 * 中断来了后先在idt[]中断描述符表中找到对应中断
 * 然后跳转中断处理函数intrXXentry(处理器会自动压栈一些寄存器和error_code)
 * 此为入口，进入后调用的是idt_func_table中的处理程序*/
intr_handler idt_func_table[IDT_DESC_CNT];					
extern intr_handler intr_entry_table[IDT_DESC_CNT];	//声明引用定义在kernel.s,中断处理函数入口数组

/* 初始化可编程中断控制器8259A
 * icw1和ocw2~3写入偶地址端口且有分辨位
 * icw2~4初始化时按顺序写入奇地址端口，可以分辨
 * 初始化之后才设置ocw1，写入奇地址端口，可以分辨*/ 
static void pic_init(void)
{
	/*初始化主片*/
	outb(PIC_M_CTRL, 0x11);		// ICW1: 边沿触发,级联8259, 需要ICW4
	outb(PIC_M_DATA, 0x20);		// ICW2: 起始中断向量号为0x20,也就是IR[0-7]为0x20 ～ 0x27
	outb(PIC_M_DATA, 0x04);		// ICW3: IR2接从片		
	outb(PIC_M_DATA, 0x01);		// ICW4: 8086模式, 正常EOI
	
	/*初始化从片*/
	outb(PIC_S_CTRL, 0x11);		// ICW1: 边沿触发,级联8259, 需要ICW4
	outb(PIC_S_DATA, 0x28);		// ICW2: 起始中断向量号为0x28,也就是IR[8-15]为0x28 ～ 0x2F
	outb(PIC_S_DATA, 0x02);		// ICW3: 设置从片连接到主片的IR2引脚		
	outb(PIC_S_DATA, 0x01);		// ICW4: 8086模式, 正常EOI

	/* 主片上打开的中断有IRQ0的时钟，IRQ1的键盘和级联从片的IRQ2，其他全部关闭 */
	outb(PIC_M_DATA, 0xf8);
	/* 打开从片上的IRQ14，此引脚接收硬盘控制器的中断 */
	outb(PIC_S_DATA, 0xbf);	

	put_str("	pic_init done\n");
}

/* 创建中断门描述符,function为中断入口地址 */
static void make_idt_desc(struct gate_desc* p_gdesc, uint8_t attr, intr_handler function)
{
	p_gdesc->func_offset_low_word = (uint32_t)function & 0x0000ffff;
	p_gdesc->selector = SELECTOR_K_CODE;
	p_gdesc->dcount = 0;
	p_gdesc->attribute = attr;
	p_gdesc->func_offset_high_word = ((uint32_t)function & 0xffff0000) >> 16;
}

/* 初始化中断描述符表*/
static void idt_desc_init(void)
{
	int i, lastidx = IDT_DESC_CNT - 1;
	for(i = 0; i < IDT_DESC_CNT; i++) {
		make_idt_desc(&idt[i], IDT_DESC_ATTR_DPL0, intr_entry_table[i]);
	} 
	// 初始化系统调用所用中断的中断描述符
	make_idt_desc(&idt[lastidx], IDT_DESC_ATTR_DPL3, syscall_handler);
	put_str("idt_desc_init done\n");
}

/* 通用的中断处理函数，一般用在异常出现时的处理 */
static void general_intr_handler(uint8_t vec_nr)	
{
	if (vec_nr == 0x27 || vec_nr == 0x2f) {
/* IRQ7 和IRQ15会产生伪中断(spurious intrrupt),无需处理 
 * 0x2f 是从片8259A上的最后一个IRQ引脚,保留项*/
		return;
	}

/* 将光标置为0，从屏幕左上角清出一片打印异常信息的区域，方便阅读 */
	set_cursor(0);
	int cursor_pos = 0;
	while (cursor_pos < 320) {	
		put_char(' ');
		cursor_pos++;
	}

	set_cursor(0);		//重置光标为屏幕左上角
	put_str("!!!!!!!!    excetion message begin     !!!!!!!!\n");
	set_cursor(88);		//从第 2 行第 8 个字符开始打印
	put_str(intr_name[vec_nr]);

/* 若为 Pagefault，将缺失的地址打印出来并悬停 */
	if (vec_nr == 14) {
		uint32_t page_fault_vaddr = 0;
		asm ("movl %%cr2, %0;" : "=r"(page_fault_vaddr));	// cr2 是存放造成page_fault的地址
		put_str("\npage fault addr is:");
		put_int(page_fault_vaddr);

	}
	put_str("\n!!!!!!!!    excetion message end     !!!!!!!!\n");

// 能进入中断处理程序就表示已经处在关中断情况下 
// 不会出现调度进程的情况。故下面的死循环不会再被中断
	while (1);
}

/* 完成一般中断处理函数注册及异常名称注册 */
static void exception_init(void)
{
	int i;
	for (i = 0; i < IDT_DESC_CNT; i++) {
/* idt_func_table 数组中的函数是在进入中断后根据中断向量号调用的
 * 见kernel/kernel.s的call [idt_func_table + %1*4]
 * 默认为general_intr_handler以后会由
 * register_handler来注册具体处理函数*/
		idt_func_table[i] = general_intr_handler;
		intr_name[i] = "unknown";		//先统一赋值为unknown,保证intr_name[20～32]不指空
	}
	intr_name[0] = "#DE Divide Error";						//除法错误
	intr_name[1] = "#DB Debug Exception"; 					//调试异常
	intr_name[2] = "NMI intrrupt";							//不可屏蔽中断
	intr_name[3] = "#BP Breakpoint	Exception";				//断点异常
	intr_name[4] = "OF Overflow Exception";					//溢出异常	
	intr_name[5] = "BR Band Range Exceeded Exception";		//超出范围异常 
	intr_name[6] = "#UD Invalid Opcode Exception";			//无效操作码异常
	intr_name[7] = "#NM Device Not Available Exception";	//设备不可用异常
	intr_name[8] = "#DF Double Fault Exception";			//双重故障异常
	intr_name[9] = "Coprocessor Segment Overrun";			//协处理器段溢出
	intr_name[10] = "#TS Invalid TSS Exception";			//无效TSS异常
	intr_name[11] = "#NP Segment Not Present";				//段不存在异常
	intr_name[12] = "#SS Stack Fault Exception";			//栈故障异常
	intr_name[13] = "#GP General Protection Exception";		//一般保护异常
	intr_name[14] = "#PF Page-Fault Exception";				//页故障异常
	// intr_name[15] 第 15 项是intel保留项,未使用
	intr_name[16] = "#MF x87 FPU Floating-Point Error";		//x87 FPU浮点错误	
	intr_name[17] = "#AC Alignment Check Exception";		//对齐检查异常
	intr_name[18] = "#MC Machine-Check Exception";			//机器检查异常
	intr_name[19] = "#XF SIMD Floating-Point Exception";	//SIMD浮点异常
}

/* 在中断处理程序数组第vector_no个元素中注册安装中断处理程序function */
void register_handler(uint8_t vector_no, intr_handler function)
{
	/* idt_func_table 数组中的函数是在进入中断后根据中断向量号调用的的call [idt_func_table + %1*4] */
	idt_func_table[vector_no] = function;
}

/* 开中断并返回开中断前的状态*/
enum intr_status intr_enable()
{
	enum intr_status old_status;
	if (intr_get_status() == INTR_ON) {
		old_status = INTR_ON;
		return old_status;
	} else {
		old_status = INTR_OFF;
		asm volatile ("sti");					//开中断，sti指令将IF位置1
		return old_status;
	}
}

/* 关中断并返回关中断前的状态*/
enum intr_status intr_disable()
{
	enum intr_status old_status;
	if (intr_get_status() == INTR_OFF) {
		old_status = INTR_OFF;
		return old_status;
	} else {
		old_status = INTR_ON;
		asm volatile ("cli" : : : "memory");	//关中断，cli指令将IF位置0 
		return old_status;
	}
}

/* 得到当前中断状态*/
enum intr_status intr_get_status()
{
	uint32_t eflags = 0;
	GET_EFLAGS(eflags);
	return (EFLAGS_IF & eflags) ? INTR_ON : INTR_OFF;
}

/* 设置中断状态*/
enum intr_status intr_set_status(enum intr_status status)
{
	return (status & INTR_ON) ? intr_enable() : intr_disable();
}

/* 完成有关中断的所有初始化工作*/
void idt_init()
{
	put_str("idt_init start\n");
	idt_desc_init();	// 初始化中断描述符表
	exception_init();	// 异常名初始化并注册通常的中断处理函数
	pic_init();			// 初始化8259A 


	/* 
	加载idt 
	由于指针只能转换成相同大小的整型，
	故32位的指针不能直接转换成64位的整型
	先将其转换成uint32_t,再将其转换成uint64_t
	*/
	uint64_t idt_operand = ((sizeof(idt) - 1) | ((uint64_t)(uint32_t)idt << 16));

	/* ATT风格这里%0就是内存地址寻址，而不是立即数或idt_operand的值*/
	asm volatile ("lidt %0" : : "m"(idt_operand));
	put_str("idt_init done\n");
}