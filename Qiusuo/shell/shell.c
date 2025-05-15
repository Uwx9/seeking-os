#include "shell.h"
#include "debug.h"
#include "global.h"
#include "syscall.h"
#include "file.h"
#include "string.h"
#include "stdio.h"
#include "syscall.h"
#include "console.h"
#include "buildin_cmd.h"

#define cmd_len 128		// 最大支持键入128个字符的命令行输入
#define MAX_ARG_NR 16	// 命令名外，最多支持15个参数


static char cmd_line[cmd_len];	// 存储输入的命令 
char cwd_cache[MAX_PATH_LEN] = {0};		// 用来记录当前目录，是当前目录的缓存，每次执行cd命令时会更新此内容
char* argv[MAX_ARG_NR];			// 全局变量，为了以后exec的程序可访问参数

int32_t argc = -1;
char final_path[MAX_PATH_LEN];


/* 输出提示符 */
static void print_promat(void)
{
	str_color = 11;	// 这样做应该有同步的问题,可能需要关中断
	printf("[unique9@localhost %s]$", cwd_cache);
	str_color = 7;
}

/* 从键盘缓冲区中最多读入count个字节到buf */
static void readline(char* buf, uint32_t count)
{
	ASSERT(buf != NULL && count > 0);
	char* pos = buf;

	while ((read(stdin_no, pos, 1) != -1) && (uint32_t)(pos - buf) < count) {	
		// 在不出错情况下，直到找到回车符才返回
		switch (*pos) {
			/* 找到回车或换行符后认为键入的命令结束，直接返回,并把pos处的回车改为0以表示命令输入完 */
			case '\n':
			case '\r':
				*pos = 0;	 	// 添加 cmd_line 的终止字符 0 
				putchar('\n');
				return;
			
			case '\b':
				if (buf[0] != '\b') {
					--pos;	// 退回到缓冲区 cmd_line 中上一个字符, 下次read应该会把上一个字符覆盖
					putchar('\b');
				}
				break;
			
			/* ctrl+l 清屏 */
			case 'l' - 'a':
				// 1 把当前字符置为0，待会儿要把这个字符前的输入再打出来	
				*pos = 0;
				// 2 清屏
				clear();
				// 3 打印提示符
				print_promat();
				// 4 将之前键入的内容再次打印
				printf("%s", buf);
				break;

			/* ctrl+u 清掉输入 */
			case 'u' - 'a':
				while (pos != buf) {
					putchar('\b');
					*(pos--) = 0;
				}
				break;

			/* 非控制键则输出字符 */
			default:
				putchar(*pos);
				pos++;
		}
	}
	printf("readline: can`t find enter_key in the cmd_line, max num of char is 128\n");
}

/* 分析字符串cmd_str中以token为分隔符的单词，将各单词的指针存入argv数组 */
static int32_t cmd_parse(char* cmd_str, char** argv, char token)
{
	ASSERT(cmd_str != NULL);
	// 每次解析将argv数组清空
	int32_t arg_idx = 0;
	while (arg_idx < MAX_ARG_NR) {
		argv[arg_idx] = NULL;
		arg_idx++;
	}

	char* next = cmd_str;
	int32_t argc = 0;
	/* 外层循环处理整个命令行 */
	while (*next) {
		// 去除命令字或参数之间的空格 
		while (*next == token) {
			next++;
		}
		// 此时不是空格，要么是命令的字符要么是0结束
		if (*next == 0) {
			break;
		}

		argv[argc] = next;
		/* 内层循环处理命令行中的每个命令字及参数 */
		while (*next && *next != token) {
			next++;
		}
		// 此时next所指要么是结束符0要么是空格
		if (*next) {	// 如果是空格
			*next++ = 0;	// 将这个单词后面的字符变为0方便从argv里面的地址读单词，再将next+1， next所指可能是0或字符
		}

		// 避免 argv 数组访问越界，参数过多则返回-1
		if (argc > MAX_ARG_NR) {
			return -1;
		}
		argc++;
	}
	return argc;
}


/* 简单的shell */
void my_shell(void)
{
	cwd_cache[0] = '/';
	while (1) {
		print_promat();
		memset(argv, 0, MAX_ARG_NR * sizeof(char*));
		memset(final_path, 0, MAX_PATH_LEN);
		memset(cmd_line, 0, cmd_len);
		readline(cmd_line, cmd_len);
		// 若只键入了一个回车
		if (cmd_line[0] == 0) {
			continue;
		}
		argc = -1;
		argc = cmd_parse(cmd_line, argv, ' ');
		if (argc == -1) {
			printf("num of arguments exceed %d\n", MAX_ARG_NR);
			continue;
		}
		
		if (!strcmp("ls", argv[0])) {
			buildin_ls(argc, argv);
		} else if (!strcmp("cd", argv[0])) {
			if (buildin_cd(argc, argv) != NULL) {
				memset(cwd_cache, 0, MAX_PATH_LEN);
				strcpy(cwd_cache, final_path);
			}
		} else if (!strcmp("pwd", argv[0])) {
			buildin_pwd(argc, argv);
		} else if (!strcmp("ps", argv[0])) {
			buildin_ps(argc, argv);
		} else if (!strcmp("clear", argv[0])) {
			buildin_clear(argc, argv);
		} else if (!strcmp("mkdir", argv[0])) {
			buildin_mkdir(argc, argv);
		} else if (!strcmp("rmdir", argv[0])) {
			buildin_rmdir(argc, argv);
		} else if (!strcmp("rm", argv[0])) {
			buildin_rm(argc, argv);
		} else {
			printf("external command\n");
		}

	}
	PANIC("my_shell: should not be here");
}