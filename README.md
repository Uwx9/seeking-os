### 这里是源码，有bochs的仓库请看https://gitee.com/yustarxin/qiusuo_os--packed

## 环境:
```
bochs版本 2.6.8
编译器: gcc version 4.8.5 (Ubuntu 4.8.5-4ubuntu2)
汇编器: NASM version 2.14.02
链接器: GNU ld (GNU Binutils for Ubuntu) 2.34
make:   GNU Make 4.2.1
``` 

## 关于文件:
可以在command目录中创建文件并利用shell脚本编译链接生成二进制文件, 该脚本会将二进制文件写入裸盘hd60M.img(跨过kernel.bin在裸盘中的位置)  
然后在kernel/main的main函数中使用sys_opne()创建文件, 参数可以用 '|' 组合, 再使用ide_read将裸盘里的文件读到申请的内存中  
最后将内存中的文件使用sys_write写入到磁盘上的文件并回收内存

---

## 一些介绍：

**Bochs软件**：是一款开源的 x86/x86-64 模拟器 (Emulator)，主要用于模拟完整的 PC 硬件环境。    
**两个硬盘**：  
- `hd60M.img` 没有文件系统  
- `hd80M.img` 已分区并建立了文件系统  
---

## Qiusuo 目录结构

```
Qiusuo/
├── boot/         # 引导程序
├── build/        # 所有编译和链接后的文件，由外部的Makefile生成，内核kernel.bin在此
├── build_gdb/    # 所有编译和链接后的文件, 用于bochs-gdb，由Qiusuo/makefile的规则生成，内核kernel.bin在此
├── command/      # 外部程序放在这里
├── device/       # 外部设备驱动，终端输出和环形队列缓冲区
├── fs/           # 文件系统
├── kernel/       # kernel，内存管理、初始化、中断等核心代码
├── lib/          # 一些库函数，系统调用
│   ├── kernel/
│   ├── usr/
│   └── ...
├── shell/        # shell，系统交互
├── thread/       # 线程
├── userprog/     # 用户进程
└── makefile      # 内部的makefile，用于gdb调试
```

---

## 编译说明

### 若想为bochs生成kernel.bin，请在 `Qiusuo_os--packed` 目录下输入：

```bash
make reall（清空build里的文件并重新编译链接后放在build中，**为什么不用 `make all`**？因为依赖关系太多没写全）
```

### 若想为bochs-gdb(您的或许是别的名字) 生成kernel.bin，请在 `Qiusuo` 目录下输入：

```bash
make reall（清空build里的文件并重新编译链接后放在build中）

```

---
## 用户名
虽然并没有用户系统, 但是终端上的名字更改在Qiusuo/shell/shell.c中的print_promat函数中,颜色的话请看表格

## RGB颜色与亮度对应表(第一行是表示颜色的一字节的前4位,后4位是背景色和控制闪烁)
##R, G, B, I 分别为第 2, 1, 0, 3位
| R | G | B | 颜色 (I=0) | 颜色 (I=1) |
|:-:|:-:|:-:|:-----------:|:-----------:|
| 0 | 0 | 0 | 黑         | 灰         |
| 0 | 0 | 1 | 蓝         | 浅蓝       |
| 0 | 1 | 0 | 绿         | 浅绿       |
| 0 | 1 | 1 | 青         | 浅青       |
| 1 | 0 | 0 | 红         | 浅红       |
| 1 | 0 | 1 | 品红       | 浅品红     |
| 1 | 1 | 0 | 棕         | 黄         |
| 1 | 1 | 1 | 白         | 亮白       |

