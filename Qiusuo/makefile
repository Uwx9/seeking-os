######   为GDB准备的makefile   ######

BUILD_DIR = ./build_gdb
ENTRY_POINT = 0xc0001300
HD60M_PATH = /home/yu/Desktop/bochs-gdb/hd60M.img

CC = gcc
AS = nasm
LD = ld 

LIB = -I  lib -I  lib/kernel -I  lib/usr -I  device/ -I kernel/ -I thread/ -I userprog/

ASFLAGS = -f elf -g
CFLAGS = -c -m32 -Wall -fno-stack-protector $(LIB) -fno-builtin -W -Wstrict-prototypes -Wmissing-prototypes -g
LDFLAGS = -m elf_i386 -Ttext $(ENTRY_POINT) -e main -Map $(BUILD_DIR)/kernel.map 

OBJS = $(BUILD_DIR)/main.o 		$(BUILD_DIR)/init.o 	$(BUILD_DIR)/interrupt.o	\
	   $(BUILD_DIR)/timer.o		$(BUILD_DIR)/kernel.o	$(BUILD_DIR)/print.o 	 	\
	   $(BUILD_DIR)/debug.o		$(BUILD_DIR)/string.o 	$(BUILD_DIR)/bitmap.o 		\
	   $(BUILD_DIR)/memory.o 	$(BUILD_DIR)/list.o		$(BUILD_DIR)/thread.o 		\
	   $(BUILD_DIR)/switch.o	$(BUILD_DIR)/console.o 	$(BUILD_DIR)/sync.o 		\
	   $(BUILD_DIR)/keyboard.o	$(BUILD_DIR)/ioqueue.o 	$(BUILD_DIR)/tss.o 			\
	   $(BUILD_DIR)/process.o	$(BUILD_DIR)/syscall.o	$(BUILD_DIR)/syscall-init.o \
	   $(BUILD_DIR)/stdio.o		$(BUILD_DIR)/ide.o 		$(BUILD_DIR)/stdio-kernel.o
	   
########################         C代码编译        ########################
$(BUILD_DIR)/main.o: kernel/main.c kernel/debug.h kernel/init.h	thread/thread.h	\
	lib/stdint.h  lib/kernel/print.h lib/kernel/list.h device/console.h device/keyboard.h 	\
	device/ioqueue.h userprog/userprog.h lib/stdio.h 
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/init.o: kernel/init.c kernel/init.h kernel/interrupt.h  device/timer.h		\
	lib/stdint.h device/console.h device/keyboard.h userprog/tss.h kernel/init.h 	\
	device/ide.h 
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/interrupt.o: kernel/interrupt.c kernel/interrupt.h kernel/global.h		\
	lib/stdint.h  lib/kernel/io.h  lib/kernel/print.h
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/timer.o:  device/timer.c  device/timer.h kernel/debug.h kernel/interrupt.h		\
	lib/kernel/print.h  lib/kernel/io.h  lib/stdint.h
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/debug.o: kernel/debug.c kernel/debug.h kernel/interrupt.h		\
	lib/kernel/print.h  lib/stdint.h
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/string.o:  lib/string.c  lib/string.h kernel/debug.h kernel/global.h		\
	lib/stdint.h 
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/bitmap.o:  lib/kernel/bitmap.c  lib/kernel/bitmap.h  lib/stdint.h  lib/string.h 	\
	kernel/interrupt.h kernel/debug.h kernel/global.h 
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/memory.o: kernel/memory.c kernel/memory.h kernel/debug.h kernel/global.h	\
	lib/stdint.h  lib/kernel/print.h  lib/kernel/bitmap.h  lib/string.h thread/sync.h 	\
	lib/kernel/list.h kernel/interrupt.h kernel/debug.h 
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/list.o: lib/kernel/list.c lib/kernel/list.h kernel/interrupt.h kernel/debug.h	\
	lib/kernel/print.h
	$(CC) $(CFLAGS) -o $@ $<
	
$(BUILD_DIR)/thread.o: thread/thread.c thread/thread.h lib/string.h kernel/global.h 	\
	lib/stdint.h kernel/memory.h kernel/interrupt.h lib/kernel/print.h kernel/debug.h	\
	thread/sync.h 	
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/console.o: device/console.c device/console.h lib/kernel/print.h thread/thread.h lib/stdint.h	\
	thread/sync.h 
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/sync.o: thread/sync.c thread/sync.h kernel/interrupt.h kernel/debug.h thread/thread.h  
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/keyboard.o: device/keyboard.c device/keyboard.h kernel/interrupt.h lib/kernel/io.h 	\
	lib/kernel/print.h kernel/global.h device/ioqueue.h 
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/ioqueue.o: device/ioqueue.c device/ioqueue.h kernel/interrupt.h kernel/global.h		\
	kernel/debug.h 
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/tss.o: userprog/tss.c userprog/tss.h lib/kernel/print.h lib/string.h kernel/global.h \
	thread/thread.h 
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/process.o: userprog/process.c userprog/process.h userprog/userprog.h thread/thread.h 	\
	lib/stdint.h kernel/global.h kernel/debug.h userprog/tss.h kernel/interrupt.h 		\
	lib/kernel/print.h lib/kernel/list.h lib/string.h kernel/memory.h 
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/syscall.o: lib/usr/syscall.c lib/usr/syscall.h lib/usr/syscall-init.h lib/stdint.h 
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/syscall-init.o: lib/usr/syscall-init.c lib/usr/syscall-init.h lib/stdint.h lib/kernel/print.h	\
	thread/thread.h lib/usr/syscall.h device/console.h lib/string.h 
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/stdio.o: lib/stdio.c lib/stdio.h lib/stdint.h lib/string.h kernel/global.h		\
	lib/usr/syscall.h 
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/ide.o: device/ide.c device/ide.h lib/kernel/stdio-kernel.h lib/stdint.h kernel/debug.h	\
	kernel/global.h lib/kernel/io.h device/timer.h kernel/interrupt.h kernel/memory.h 		\
	lib/stdio.h lib/string.h 
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/stdio-kernel.o: lib/kernel/stdio-kernel.c lib/kernel/stdio-kernel.h kernel/global.h lib/stdio.h device/console.h \
	lib/stdint.h 
	$(CC) $(CFLAGS) -o $@ $<

########################         汇编代码编译        ########################
########        boot        #######
$(BUILD_DIR)/mbr.bin: boot/mbr.s boot/include/boot.inc
	nasm -I  boot/include/ -o $@ $<
  
$(BUILD_DIR)/loader.bin: boot/loader.s boot/include/boot.inc
	nasm -I  boot/include/ -o $@ $<

########        other asm        ########
$(BUILD_DIR)/kernel.o: kernel/kernel.s
	$(AS) $(ASFLAGS) -o $@ $<

$(BUILD_DIR)/print.o: lib/kernel/print.s 
	$(AS) $(ASFLAGS) -o $@ $<

$(BUILD_DIR)/switch.o: thread/switch.s 
	$(AS) $(ASFLAGS) -o $@ $<

########################         链接所有目标文件        ########################
$(BUILD_DIR)/kernel.bin: $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

.PHONY: mk_dir boot build hd clean all reall 

mk_dir:
	if [ ! -d "$(BUILD_DIR)" ]; then mkdir -p "$(BUILD_DIR)"; fi

boot: mk_dir $(BUILD_DIR)/mbr.bin $(BUILD_DIR)/loader.bin	

build: mk_dir $(BUILD_DIR)/kernel.bin

hd:
	dd if=$(BUILD_DIR)/mbr.bin of=$(HD60M_PATH) bs=512 count=1 conv=notrunc 
	dd if=$(BUILD_DIR)/loader.bin of=$(HD60M_PATH) bs=512 count=4 seek=2 conv=notrunc
	dd if=$(BUILD_DIR)/kernel.bin of=$(HD60M_PATH) bs=512 count=233 seek=9 conv=notrunc

#生成可以被GDB理解的符号表，用于GDB调试
gdb_symbol:
	objcopy --only-keep-debug $(BUILD_DIR)/kernel.bin $(BUILD_DIR)/kernel.sym

clean:
	cd $(BUILD_DIR) && rm -f ./*

all: mk_dir boot build hd gdb_symbol

reall: clean all

