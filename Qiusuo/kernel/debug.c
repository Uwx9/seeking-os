#include "debug.h"
#include "print.h"
#include "interrupt.h"

void panic_spin(char* filename,
                int line,
                const char* func, 
                const char* condition)
{
    intr_disable();     //关中断的原因应该是因为程序出错不希望中断此刻来，防止CPU被调走
    put_str("\n\n\n!!!!! error !!!!!\n\n", 0x07);
    put_str("filename:", 0x07); put_str(filename, 0x07); put_str("\n", 0x07);
    put_str("line:0x", 0x07); put_int(line); put_str("\n", 0x07);
    put_str("func:", 0x07); put_str((char*)func, 0x07); put_str("\n", 0x07);  //类型转换原因可能是put_str参数为char*，这里是const char*
    put_str("condition:", 0x07); put_str((char*)condition, 0x07); put_str("\n", 0x07);
    /*big question: put_str('\n')没法换行，必须"\n", 0x07才行，也对，putchar拿的是asc码，putstr参数是地址
     *传一个\n的asc码给putstr被当成字符串首地址就错了*/
    while(1);
}   