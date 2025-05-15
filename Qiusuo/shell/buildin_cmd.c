#include "buildin_cmd.h"
#include "string.h"
#include "debug.h"
#include "fs.h"
#include "syscall.h"
#include "global.h"
#include "stdio.h"
#include "syscall.h"
#include "dir.h"

extern char final_path[MAX_PATH_LEN];

/* 将路径old_abs_path中的..和.转换为实际路径后存入new_abs_path */
static void wash_path(char* old_abs_path, char* new_abs_path)
{
	ASSERT(old_abs_path[0] == '/');
	char name[MAX_PATH_LEN] = {0};
	char* sub_path = old_abs_path;
	sub_path = path_parse(sub_path, name);
	if (name[0] == 0) {		// 只键入了'/'
		new_abs_path[0] = '/';
		new_abs_path[1] = 0;
		return;
	}

	new_abs_path[0] = 0;	// 避免传给 new_abs_path 的缓冲区不干净
	strcat(new_abs_path, "/");
	while (name[0]) {	
		// 如果是上一级目录“..”
		if (!strcmp("..", name)) {
			char* slash_ptr = strrchr(new_abs_path, '/');
			/* 如果未到 new_abs_path 中的顶层目录，就将最右边的'/'替换为0，
			这样便去除了new_abs_path中最后一层路径，相当于到了上一级目录 */
			if (slash_ptr != new_abs_path) {
				*slash_ptr = 0;
			} else {	// 已经到了顶层目录
				*(slash_ptr + 1) = 0;
			}
		} else if (strcmp(".", name)) {	// 如果路径不是‘.’，就将name拼接到new_abs_path
			// 如果 new_abs_path 不是"/", 就拼接一个"/",此处的判断是为了避免路径开头变成这样"//"
			if (strcmp(new_abs_path, "/")) {
				strcat(new_abs_path, "/");
			}
			strcat(new_abs_path, name);
		}	// 若 name 为当前目录"."，无需处理new_abs_path

		/* 继续遍历下一层路径 */
		memset(name, 0, MAX_PATH_LEN);
		if (sub_path) {
			sub_path = path_parse(sub_path, name);
		}
	
	}
}

/* 将 path 处理成不含..和.的绝对路径，存储在final_path */
void make_clear_abs_path(char* path, char* final_path)
{
	char abs_path[MAX_PATH_LEN] = {0};
	/* 先判断是否输入的是绝对路径,若输入的不是绝对路径，就拼接成绝对路径 */
	if (path[0] != '/') {
		memset(abs_path, 0, MAX_PATH_LEN);
		if (getcwd(abs_path, MAX_PATH_LEN) != NULL) {
			if (!((abs_path[0] == '/') && (abs_path[1] == 0))) {
				// 若abs_path表示的当前目录不是根目录
				strcat(abs_path, "/");
			}
		}
	}
	strcat(abs_path, path);
	wash_path(abs_path, final_path);
}

// 这些内建函数的第一个参数是输入命令的单词数，第二个是这些单词的起始地址数组的开头

/* pwd 命令的内建函数 */
void buildin_pwd(uint32_t argc, char** argv UNUSED)
{
	if (argc != 1) {
		printf("pwd: no argument support\n");
		return;
	}

	if (getcwd(final_path, MAX_PATH_LEN) != NULL) {
		printf("%s\n", final_path);
	} else {
		printf("pwd: get current work directory failed.\n");
	}
}

/* cd 命令的内建函数 */
char* buildin_cd(uint32_t argc, char** argv)
{
	if (argc > 2) {
		printf("cd: only support 1 argument\n");
		return NULL;
	}
	/* 若是只键入cd而无参数，直接返回到根目录 */
	if (argc == 1) {
		final_path[0] = '/';
		final_path[1] = 0;
	} else {
		make_clear_abs_path(argv[1], final_path);
	}
	if (chdir(final_path) == -1) {
		printf("cd: no such directory %s\n", final_path);
		return NULL;
	}
	return final_path;
}

/* ls 命令的内建函数 */
void buildin_ls(uint32_t argc, char** argv)
{
	char* path_name = NULL;
	struct stat file_stat;
	memset(&file_stat, 0, sizeof(struct stat));
	bool long_info = false;
	uint32_t arg_path_nr = 0;
	uint32_t arg_idx = 1;		// 跨过 argv[0]，argv[0]是字符串“ls”
	while (arg_idx < argc) {	// 解析每个参数
		if (argv[arg_idx][0] == '-') {
			if (!strcmp(argv[arg_idx], "-l")) {	// 如果是参数-l
				long_info = true;
			} else if(!strcmp(argv[arg_idx], "-h")) {	// 参数-h
				printf("usage: -l list all infomation about the file.\n-h for help\nlist all files in the current dirctory if no option\n");
				return;
			} else {	// 只支持-l和-h
				printf("ls: invalid option %s\nTry `ls -h' for more information.\n", argv[arg_idx]);
				return;
			}
		} else {	// 不是-开头，当作 ls 的路径参数
			if (arg_path_nr == 0) {	// 还没有路径参数
				path_name = argv[arg_idx];
				arg_path_nr = 1;
			} else {
				printf("ls: only support one path\n");
				return;
			}
		}
		arg_idx++;
	}

	if (path_name == NULL) {	// 若只输入了 ls 或ls -l
		// 没有输入操作路径，默认以当前路径的绝对路径为参数
		if (getcwd(final_path, MAX_PATH_LEN) != NULL) {
			path_name = final_path;
		} else {
			printf("ls: getcwd for default path failed\n");
			return;
		}
	} else {
		make_clear_abs_path(path_name, final_path);
		path_name = final_path;
	}
	
	if (stat(path_name, &file_stat) == -1) {
 		printf("ls: cannot access %s: No such file or directory\n", path_name);
		return;
	}
	if (file_stat.st_filetype == FT_DIRECTORY) {
		struct dir* dir = opendir(path_name);
		struct dir_entry* dir_e = NULL;
		char subpath_name[MAX_PATH_LEN] = {0};
		uint32_t path_name_len = strlen(path_name);
		uint32_t last_char_idx = path_name_len - 1;
		memcpy(subpath_name, path_name, path_name_len);
		if (subpath_name[last_char_idx] != '/') {
			subpath_name[path_name_len] = '/';	// 添加/，后面还要把目录项接上来
			path_name_len++;
		}

		rewinddir(dir);
		if (long_info) {
			char ftype;
			printf("total: %d\n", file_stat.st_size);
			while ((dir_e = readdir(dir))) {
				ftype = 'd';
				if (dir_e->f_type == FT_REGULAR) {
					ftype = '-';
				}
				subpath_name[path_name_len] = 0;
				strcat(subpath_name, dir_e->filename);
				memset(&file_stat, 0, sizeof(struct stat));
				if (stat(subpath_name, &file_stat) == -1) {	// 将目录项的名字接在subpathname后查看信息，下次循环重新将名字接在输入路径+/后面
					printf("ls: cannot access %s: No such file or directory\n", dir_e->filename);
					return;
				}
				printf("%c  %d  %d  %s\n", ftype, dir_e->i_no, file_stat.st_size, dir_e->filename);
			}
		} else {
			while ((dir_e = readdir(dir))) {
				printf("%s ", dir_e->filename);
			}
			printf("\n");
		}
		closedir(dir);
	} else {	// 输入的路径是文件
		if (long_info) {
			printf("-  %d  %d  %s\n", file_stat.st_ino, file_stat.st_size, path_name);
		} else {
			printf("%s\n", path_name);
		}
	}

}

/* ps 命令内建函数 */
void buildin_ps(uint32_t argc, char** argv UNUSED)
{
	if (argc != 1) {
		printf("ps: no argument support!\n");
		return;
	}
	ps();
}

/* clear 命令内建函数 */
void buildin_clear(uint32_t argc, char** argv UNUSED)
{
	if (argc != 1) {
		printf("clear: no argument support!\n");
		return;
	}
	clear();
}

/* mkdir 命令内建函数 */
int32_t buildin_mkdir(uint32_t argc, char** argv)
{
	int32_t ret = -1;
	if (argc != 2) {
		printf("mkdir: only support one argument\n");
	} else {
		make_clear_abs_path(argv[1], final_path);
		// 若创建的不是根目录
		if (strcmp("/", final_path)) {
			if (mkdir(final_path) == 0) {
				ret = 0;
			} else {
				printf("mkdir: create directory %s failed.\n", argv[1]);
			}
		}
	}
	return ret;
}

/* rmdir 命令内建函数 */
int32_t buildin_rmdir(uint32_t argc, char** argv)
{
	int32_t ret = -1;
	if (argc != 2) {
		printf("rmdir: only support one argument\n");
	} else {
		if (!strcmp(".", argv[1])) {
			printf("rmdir: remove '.' failed, Invalid arguments\n");
		} else {
			make_clear_abs_path(argv[1], final_path);
			// 若删除的不是根目录
			if (strcmp("/", final_path)) {
				if (rmdir(final_path) == 0) {
					ret = 0;
				} else {
					printf("rmdir: remove %s failed.\n", argv[1]);
				}
			}
		}	
	}
	return ret;
}

/* rm 命令内建函数 */
int32_t buildin_rm(uint32_t argc, char** argv)
{
	int32_t ret = -1;
	if (argc != 2) {
		printf("rm: only support one argument\n");
	} else {
		make_clear_abs_path(argv[1], final_path);
		// 若删除的不是根目录
		if (strcmp("/", final_path)) {
			if (unlink(final_path) == 0) {
				ret = 0;
			} else {
				printf("rm: delete %s failed.\n", argv[1]);
			}
		}
	}
	return ret;
}