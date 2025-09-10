BUILD_DIR = ./build
ENTRY_POINT = 0xc0001300
HD60M_PATH = /home/yu/Desktop/bochs/hd60M.img

CC = gcc
AS = nasm
LD = ld 

LIB = -I  lib -I  lib/kernel -I  lib/usr -I  device/ -I kernel/ -I thread/ -I userprog/	\
	-I fs -I shell

ASFLAGS = -f elf 
CFLAGS = -c -m32 -Wall -fno-stack-protector $(LIB) -fno-builtin -W -Wstrict-prototypes -Wmissing-prototypes 
LDFLAGS = -m elf_i386 -Ttext $(ENTRY_POINT) -e main -Map $(BUILD_DIR)/kernel.map

OBJS = $(BUILD_DIR)/main.o 			$(BUILD_DIR)/init.o 		$(BUILD_DIR)/interrupt.o	\
	   $(BUILD_DIR)/timer.o			$(BUILD_DIR)/kernel.o		$(BUILD_DIR)/print.o 	 	\
	   $(BUILD_DIR)/debug.o			$(BUILD_DIR)/string.o 		$(BUILD_DIR)/bitmap.o 		\
	   $(BUILD_DIR)/memory.o 		$(BUILD_DIR)/list.o			$(BUILD_DIR)/thread.o 		\
	   $(BUILD_DIR)/switch.o		$(BUILD_DIR)/console.o 		$(BUILD_DIR)/sync.o 		\
	   $(BUILD_DIR)/keyboard.o		$(BUILD_DIR)/ioqueue.o 		$(BUILD_DIR)/tss.o 			\
	   $(BUILD_DIR)/process.o		$(BUILD_DIR)/syscall.o		$(BUILD_DIR)/syscall-init.o \
	   $(BUILD_DIR)/stdio.o			$(BUILD_DIR)/ide.o 			$(BUILD_DIR)/stdio-kernel.o \
	   $(BUILD_DIR)/fs.o 			$(BUILD_DIR)/dir.o 			$(BUILD_DIR)/file.o 		\
	   $(BUILD_DIR)/inode.o 		$(BUILD_DIR)/fork.o 		$(BUILD_DIR)/shell.o		\
	   $(BUILD_DIR)/buildin_cmd.o	$(BUILD_DIR)/exec.o 		$(BUILD_DIR)/assert.o		\
	   $(BUILD_DIR)/wait_exit.o 	$(BUILD_DIR)/pipe.o
	   
########################         C代码编译        ########################
$(BUILD_DIR)/main.o: kernel/main.c kernel/debug.h kernel/init.h	thread/thread.h	\
	lib/stdint.h  lib/kernel/print.h lib/kernel/list.h device/console.h device/keyboard.h 	\
	device/ioqueue.h userprog/userprog.h lib/stdio.h shell/shell.h 
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/init.o: kernel/init.c kernel/init.h kernel/interrupt.h  device/timer.h		\
	lib/stdint.h device/console.h device/keyboard.h userprog/tss.h kernel/init.h 	\
	device/ide.h fs/fs.h 
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

$(BUILD_DIR)/fs.o: fs/fs.c fs/fs.h fs/dir.h fs/inode.h fs/super_block.h kernel/global.h		\
	lib/stdint.h lib/kernel/stdio-kernel.h kernel/memory.h kernel/debug.h lib/string.h \
	lib/kernel/list.h device/console.h
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/dir.o: fs/dir.c fs/dir.h fs/inode.h fs/super_block.h kernel/global.h fs/fs.h		\
	lib/stdint.h lib/kernel/stdio-kernel.h  kernel/debug.h lib/string.h fs/file.h device/ide.h		
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/file.o: fs/file.c  fs/file.h fs/inode.h fs/super_block.h kernel/global.h fs/fs.h		\
	lib/stdint.h lib/kernel/stdio-kernel.h  kernel/debug.h lib/string.h device/ide.h	fs/dir.h		\
	thread/thread.h lib/kernel/bitmap.h 
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/inode.o: fs/inode.c fs/inode.h fs/super_block.h kernel/global.h fs/fs.h	kernel/debug.h 	\
	lib/stdint.h lib/kernel/stdio-kernel.h  kernel/debug.h lib/string.h fs/file.h device/ide.h		\
	thread/thread.h kernel/interrupt.h 
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/fork.o: userprog/fork.c userprog/fork.h lib/string.h kernel/global.h thread/thread.h \
	userprog/process.h kernel/debug.h fs/file.h fs/inode.h kernel/interrupt.h lib/kernel/list.h
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/shell.o: shell/shell.c shell/shell.h kernel/debug.h kernel/global.h lib/usr/syscall.h \
	fs/file.h lib/string.h 
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/buildin_cmd.o: shell/buildin_cmd.c shell/buildin_cmd.h lib/string.h kernel/debug.h \
	lib/usr/syscall.h fs/fs.h
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/exec.o: userprog/exec.c userprog/exec.h lib/stdint.h kernel/memory.h kernel/global.h	\
	lib/string.h fs/fs.h thread/thread.h 
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/assert.o: lib/usr/assert.c lib/usr/assert.h lib/stdio.h kernel/global.h
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/wait_exit.o: userprog/wait_exit.c userprog/wait_exit.h thread/thread.h fs/fs.h	\
	lib/usr/assert.h 
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/pipe.o: shell/pipe.c shell/pipe.h kernel/global.h fs/fs.h fs/file.h device/ioqueue.h	\
	lib/string.h 
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
	dd if=$(BUILD_DIR)/kernel.bin of=$(HD60M_PATH) bs=512 count=370 seek=9 conv=notrunc

clean:
	cd $(BUILD_DIR) && rm -f ./*

all: mk_dir boot build hd 

reall: clean all
