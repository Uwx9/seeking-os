#include "timer.h"
#include "print.h"
#include "io.h"

#define IRQ0_FREQUENCY      100
#define INPUT_FREQUENCY     1193180
#define COUNTER0_VAULE      INPUT_FREQUENCY / IRQ0_FREQUENCY
#define COUNTER0_PORT       0x40
#define COUNTER0_NO         0
#define COUNTER0_MODE       2
#define READ_WRITE_LATCH    3
#define PIT_CONTROL_PORT    0x43

/* 把操作的计数器counter_no､读写锁属性rwl､计数器模式 
 * counter_mode写入模式控制寄存器并赋予初始值counter_value */
static void frequency_set(uint8_t counter_port,        
                          uint8_t counter_no,          
                          uint8_t rwl,                 
                          uint8_t counter_mode,        
                          uint16_t counter_value)       
{
    /* 往控制字寄存器端口0x43中写入控制字 */ 
    outb(PIT_CONTROL_PORT, (uint8_t)(counter_no<<6 | rwl<<4 | counter_mode<<1));
    
    /* 写入counter_value，先低8位，后高8位 */
    outb(counter_port, (uint8_t)counter_value);
    outb(counter_port, (uint8_t)(counter_value>>8));
}

/* 初始化PIT8253 */ 
void timer_init()
{
    put_str("time_init start\n");
    frequency_set(COUNTER0_PORT, COUNTER0_NO, READ_WRITE_LATCH, COUNTER0_MODE, COUNTER0_VAULE);                     
    put_str("time_init done\n");
}
