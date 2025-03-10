#include "print.h"
void main(void)
{
    put_str("I am kernel\n"); 
    put_int(0); 
    put_char('\n'); 
    put_int(9); 
    put_char('\n'); 
    put_int(0x00021a3f); 
    put_char('\n'); 
    put_int(0x12345678); 
    put_char('\n'); 
    put_int(0x00000000);
    put_str("let's go kernel!");
    put_char('\n');
    put_char('\n');
    while(1);
}