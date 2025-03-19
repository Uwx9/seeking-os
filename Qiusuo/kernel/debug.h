#ifndef __KERNEL_DEBUG_H
#define __KERNEL_DEBUG_H

void panic_spin(char* filename, int line, const char* func, const char* condition);

/************* __VA_ARGS__  **************
 * __VA_ARGS__ 是预处理器所支持的专用标识符。
 * "..."表示定义的宏其参数可变。
 * 前3个是预定义的宏,含义和名字一样
 * __VA_ARGD__会被替换成(...),即PANIC中传入的参数*/

#define PANIC(...) panic_spin(__FILE__, __LINE__, __func__, __VA_ARGS__)    

#ifdef NDEBUG
    #define ASSERT(CONDITION) (void)0
#else
    #define ASSERT(CONDITION)                  \
    if (CONDITION) {} else {                    \
        /* 符号#让编译器将宏的参数转化为字符串字面量*/ \
        PANIC(#CONDITION);                        \
    }

#endif  //NDEBUG

#endif  //__KERNEL_DEBUG_H