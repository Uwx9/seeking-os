#include "string.h"
#include "global.h"
#include "debug.h"

/* 将 dst_起始的size个字节置为value */
void memset(void* dst_, uint8_t value, uint32_t size)
{
    ASSERT(dst_ != NULL);

/* 指针是32位，可以代表一个地址，但是解引用访问多大的位数取决于指针类型
 * void*可以表示一个地址，但无法解引用，因为不知道要访问多少位数据 
 * 这里逐字节操作，所以用访问8位的指针 */
    uint8_t* dst = (uint8_t*)dst_;  
    while (size-- > 0) {
        *dst++ = value;
    }
}

/* 将src_起始的size个字节复制到dst_ */
void memcpy(void* dst_, const void* src_, uint32_t size)
{
    
    ASSERT(dst_ != NULL && src_ != NULL);
    uint8_t* dst = (uint8_t*)dst_;              //隐式转换
    const uint8_t* src = (const uint8_t*)src_;  
    while (size-- > 0) {
        *dst++ = *src++;
    }
}

/* 连续比较以地址a_和地址b_开头的size个字节，若相等则返回0， 
 * 若a_大于b_，返回+1，否则返回−1 */ 
int memcmp(const void* a_, const void* b_, uint32_t size)
{
    ASSERT(a_ != NULL && b_ != NULL);
    const uint8_t* a = a_;
    const uint8_t* b = b_;
    while (size-- >0) {
        if (*a != *b) {
            return *a > *b ? 1 : -1; 
        }
        a++;
        b++;
    }
    return 0;
}

/* 将字符串从src_复制到dst_ */ 
char* strcpy(char* dst_, const char* src_)
{
    ASSERT(dst_ != NULL && src_ != NULL);
    char* ret = dst_;
    while ((*dst_++ = *src_++));     //真值是*dst_被赋值后的值,也就是*src_的值
    return ret;
}

/* 返回字符串长度,没包含'0' */
uint32_t strlen(const char* str)
{
    ASSERT(str != NULL);
    const char* p = str;
    while (*p++);
    return p - str - 1;
}

/* 比较两个字符串,若a_中的字符大于b_中的字符返回1,相等时返回0,否则返回−1 */ 
int8_t strcmp(const char* a, const char* b)
{
    ASSERT(a != NULL && b != NULL);
    while (*a != 0 && *a == *b) {    //*a!=0防止一直相等后继续循环
        a++;
        b++;
    }
/* 如果*a小于*b就返回−1，否则就属于*a大于等于*b的情况。 
 * 在后面的布尔表达式"*a > *b"中，* 若*a大于*b，表达式就等于1, 
 * 否则表达式不成立，也就是布尔值为0，恰恰表示*a等于*b 
 * 利用了一个语句的1和0两种真值情况 */ 
    return *a < *b ? -1 : *a > *b;
}

/* 从左到右查找字符串str中首次出现字符ch的地址*/
char* strchr(const char* str, uint8_t ch)
{
    ASSERT(str != NULL);
    while (*str != 0) {
        if (*str == ch) {
            return (char*)str;      //需要强制转化成和返回值类型一样,否则编译器会报const属性丢失
        }
        str++;
    }
    return NULL;
}

/* 从后往前查找字符串str中首次出现字符ch的地址*/
char* strrchr(const char* str, uint8_t ch)
{
    ASSERT(str != NULL);
    const char* last_char = NULL;
    while (*str != 0) {
        if (*str == ch) {
            last_char = str;
        }
        str++;
    }
    return (char*)last_char; 
}

/* 将字符串src_拼接到dst_后，返回拼接的串地址 */
char* strcat(char* dst_, const char* src_)
{
    ASSERT(dst_ != NULL && src_ != NULL);
    char* dst_tail = dst_;
    while (*dst_tail++);
    --dst_tail;
    while ((*dst_tail++ = *src_++));
    return dst_;
}

/* 在字符串str中查找字符ch出现的次数 */
uint32_t strchrs(const char* str, uint8_t ch)
{
    ASSERT(str != NULL);
    const char* p = str;
    uint32_t ch_cnt = 0;
    while (*p != 0) {
        if (*p == ch) {
            ++ch_cnt;
        }
        p++;
    }
    return ch_cnt;
}