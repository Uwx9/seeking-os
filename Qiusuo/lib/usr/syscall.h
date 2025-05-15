#ifndef __LIB_USR_SYSCALL_H
#define __LIB_USR_SYSCALL_H
#include "stdint.h"
#include "thread.h"
#include "fs.h"

enum SYS_NR{
	SYS_GETPID,
	SYS_WRITE,
	SYS_READ,
	SYS_MALLOC,
	SYS_FREE,
	SYS_FORK,
	SYS_PUTCHAR,
	SYS_CLEAN,
	SYS_GETCWD,
	SYS_OPEN,
	SYS_CLOSE,
	SYS_LSEEK,
	SYS_UNLINK,
	SYS_MKDIR,
	SYS_OPENDIR,
	SYS_CLOSEDIR,
	SYS_CHDIR,
	SYS_RMDIR,
	SYS_READDIR,
	SYS_REWINDDIR,
	SYS_STAT,
	SYS_PS
};

uint32_t getpid(void);
uint32_t write(int32_t fd, const void* buf, uint32_t count);
int32_t read(int32_t fd, void* buf, uint32_t count);
void* malloc(uint32_t size);
void free(void* ptr);
pid_t fork(void);
void putchar(const char chr);
void clear(void);
char* getcwd(char* buf, uint32_t size);
int32_t open(const char* path_name, uint8_t flag);
int32_t close(int32_t fd);
int32_t lseek(int32_t fd, int32_t offset, uint8_t whence);
int32_t unlink(const char* path_name);
int32_t mkdir(const char* path_name);
struct dir* opendir(const char* name);
int32_t closedir(struct dir* dir);
int32_t chdir(const char* path);
int32_t rmdir(const char* path_name);
struct dir_entry* readdir(struct dir* dir);
void rewinddir(struct dir* dir);
int32_t stat(const char* path, struct stat* buf);
void ps(void);
#endif 