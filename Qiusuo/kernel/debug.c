#include "debug.h"
#include "print.h"
#include "interrupt.h"

void panic_spin(char* filename,
                int line,
                const char* func, 
                const char* condition)
{
    intr_disable();     //关中断的原因应该是因为程序出错不希望中断此刻来，防止CPU被调走
    put_str("\n\n\n!!!!! error !!!!!\n\n");
    put_str("filename:"); put_str(filename); put_str("\n");
    put_str("line:0x"); put_int(line); put_str("\n");
    put_str("func:"); put_str((char*)func); put_str("\n");  //类型转换原因可能是put_str参数为char*，这里是const char*
    put_str("condition:"); put_str((char*)condition); put_str("\n");
    /*big question: put_str('\n')没法换行，必须"\n"才行，也对，putchar拿的是asc码，putstr参数是地址
     *传一个\n的asc码给putstr被当成字符串首地址就错了*/
    while(1);
}   