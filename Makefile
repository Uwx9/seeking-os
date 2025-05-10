BUILD_DIR = ./Qiusuo/build
ENTRY_POINT = 0xc0001300
HD60M_PATH = /home/yu/Desktop/bochs/hd60M.img

CC = gcc
AS = nasm
LD = ld 

LIB = -I  Qiusuo/lib -I  Qiusuo/lib/kernel -I  Qiusuo/lib/usr -I  Qiusuo/device/ -I Qiusuo/kernel/ -I Qiusuo/thread/ -I Qiusuo/userprog/	\
	-I Qiusuo/fs 

ASFLAGS = -f elf 
CFLAGS = -c -m32 -Wall -fno-stack-protector $(LIB) -fno-builtin -W -Wstrict-prototypes -Wmissing-prototypes 
LDFLAGS = -m elf_i386 -Ttext $(ENTRY_POINT) -e main -Map $(BUILD_DIR)/kernel.map

OBJS = $(BUILD_DIR)/main.o 		$(BUILD_DIR)/init.o 	$(BUILD_DIR)/interrupt.o	\
	   $(BUILD_DIR)/timer.o		$(BUILD_DIR)/kernel.o	$(BUILD_DIR)/print.o 	 	\
	   $(BUILD_DIR)/debug.o		$(BUILD_DIR)/string.o 	$(BUILD_DIR)/bitmap.o 		\
	   $(BUILD_DIR)/memory.o 	$(BUILD_DIR)/list.o		$(BUILD_DIR)/thread.o 		\
	   $(BUILD_DIR)/switch.o	$(BUILD_DIR)/console.o 	$(BUILD_DIR)/sync.o 		\
	   $(BUILD_DIR)/keyboard.o	$(BUILD_DIR)/ioqueue.o 	$(BUILD_DIR)/tss.o 			\
	   $(BUILD_DIR)/process.o	$(BUILD_DIR)/syscall.o	$(BUILD_DIR)/syscall-init.o \
	   $(BUILD_DIR)/stdio.o		$(BUILD_DIR)/ide.o 		$(BUILD_DIR)/stdio-kernel.o \
	   $(BUILD_DIR)/fs.o 		$(BUILD_DIR)/dir.o 		$(BUILD_DIR)/file.o 		\
	   $(BUILD_DIR)/inode.o 
	   
########################         C代码编译        ########################
$(BUILD_DIR)/main.o: Qiusuo/kernel/main.c Qiusuo/kernel/debug.h Qiusuo/kernel/init.h	Qiusuo/thread/thread.h	\
	Qiusuo/lib/stdint.h  Qiusuo/lib/kernel/print.h Qiusuo/lib/kernel/list.h Qiusuo/device/console.h Qiusuo/device/keyboard.h 	\
	Qiusuo/device/ioqueue.h Qiusuo/userprog/userprog.h Qiusuo/lib/stdio.h 
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/init.o: Qiusuo/kernel/init.c Qiusuo/kernel/init.h Qiusuo/kernel/interrupt.h  Qiusuo/device/timer.h		\
	Qiusuo/lib/stdint.h Qiusuo/device/console.h Qiusuo/device/keyboard.h Qiusuo/userprog/tss.h Qiusuo/kernel/init.h 	\
	Qiusuo/device/ide.h Qiusuo/fs/fs.h 
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/interrupt.o: Qiusuo/kernel/interrupt.c Qiusuo/kernel/interrupt.h Qiusuo/kernel/global.h		\
	Qiusuo/lib/stdint.h  Qiusuo/lib/kernel/io.h  Qiusuo/lib/kernel/print.h
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/timer.o:  Qiusuo/device/timer.c  Qiusuo/device/timer.h Qiusuo/kernel/debug.h Qiusuo/kernel/interrupt.h		\
	Qiusuo/lib/kernel/print.h  Qiusuo/lib/kernel/io.h  Qiusuo/lib/stdint.h
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/debug.o: Qiusuo/kernel/debug.c Qiusuo/kernel/debug.h Qiusuo/kernel/interrupt.h		\
	Qiusuo/lib/kernel/print.h  Qiusuo/lib/stdint.h
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/string.o:  Qiusuo/lib/string.c  Qiusuo/lib/string.h Qiusuo/kernel/debug.h Qiusuo/kernel/global.h		\
	Qiusuo/lib/stdint.h 
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/bitmap.o:  Qiusuo/lib/kernel/bitmap.c  Qiusuo/lib/kernel/bitmap.h  Qiusuo/lib/stdint.h  Qiusuo/lib/string.h 	\
	Qiusuo/kernel/interrupt.h Qiusuo/kernel/debug.h Qiusuo/kernel/global.h 
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/memory.o: Qiusuo/kernel/memory.c Qiusuo/kernel/memory.h Qiusuo/kernel/debug.h Qiusuo/kernel/global.h	\
	Qiusuo/lib/stdint.h  Qiusuo/lib/kernel/print.h  Qiusuo/lib/kernel/bitmap.h  Qiusuo/lib/string.h Qiusuo/thread/sync.h 	\
	Qiusuo/lib/kernel/list.h Qiusuo/kernel/interrupt.h Qiusuo/kernel/debug.h 
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/list.o: Qiusuo/lib/kernel/list.c Qiusuo/lib/kernel/list.h Qiusuo/kernel/interrupt.h Qiusuo/kernel/debug.h	\
	Qiusuo/lib/kernel/print.h
	$(CC) $(CFLAGS) -o $@ $<
	
$(BUILD_DIR)/thread.o: Qiusuo/thread/thread.c Qiusuo/thread/thread.h Qiusuo/lib/string.h Qiusuo/kernel/global.h 	\
	Qiusuo/lib/stdint.h Qiusuo/kernel/memory.h Qiusuo/kernel/interrupt.h Qiusuo/lib/kernel/print.h Qiusuo/kernel/debug.h	\
	Qiusuo/thread/sync.h 	
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/console.o: Qiusuo/device/console.c Qiusuo/device/console.h Qiusuo/lib/kernel/print.h Qiusuo/thread/thread.h Qiusuo/lib/stdint.h	\
	Qiusuo/thread/sync.h 
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/sync.o: Qiusuo/thread/sync.c Qiusuo/thread/sync.h Qiusuo/kernel/interrupt.h Qiusuo/kernel/debug.h Qiusuo/thread/thread.h  
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/keyboard.o: Qiusuo/device/keyboard.c Qiusuo/device/keyboard.h Qiusuo/kernel/interrupt.h Qiusuo/lib/kernel/io.h 	\
	Qiusuo/lib/kernel/print.h Qiusuo/kernel/global.h Qiusuo/device/ioqueue.h 
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/ioqueue.o: Qiusuo/device/ioqueue.c Qiusuo/device/ioqueue.h Qiusuo/kernel/interrupt.h Qiusuo/kernel/global.h		\
	Qiusuo/kernel/debug.h 
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/tss.o: Qiusuo/userprog/tss.c Qiusuo/userprog/tss.h Qiusuo/lib/kernel/print.h Qiusuo/lib/string.h Qiusuo/kernel/global.h \
	Qiusuo/thread/thread.h 
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/process.o: Qiusuo/userprog/process.c Qiusuo/userprog/process.h Qiusuo/userprog/userprog.h Qiusuo/thread/thread.h 	\
	Qiusuo/lib/stdint.h Qiusuo/kernel/global.h Qiusuo/kernel/debug.h Qiusuo/userprog/tss.h Qiusuo/kernel/interrupt.h 		\
	Qiusuo/lib/kernel/print.h Qiusuo/lib/kernel/list.h Qiusuo/lib/string.h Qiusuo/kernel/memory.h 
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/syscall.o: Qiusuo/lib/usr/syscall.c Qiusuo/lib/usr/syscall.h Qiusuo/lib/usr/syscall-init.h Qiusuo/lib/stdint.h 
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/syscall-init.o: Qiusuo/lib/usr/syscall-init.c Qiusuo/lib/usr/syscall-init.h Qiusuo/lib/stdint.h Qiusuo/lib/kernel/print.h	\
	Qiusuo/thread/thread.h Qiusuo/lib/usr/syscall.h Qiusuo/device/console.h Qiusuo/lib/string.h 
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/stdio.o: Qiusuo/lib/stdio.c Qiusuo/lib/stdio.h Qiusuo/lib/stdint.h Qiusuo/lib/string.h Qiusuo/kernel/global.h		\
	Qiusuo/lib/usr/syscall.h 
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/ide.o: Qiusuo/device/ide.c Qiusuo/device/ide.h Qiusuo/lib/kernel/stdio-kernel.h Qiusuo/lib/stdint.h Qiusuo/kernel/debug.h	\
	Qiusuo/kernel/global.h Qiusuo/lib/kernel/io.h Qiusuo/device/timer.h Qiusuo/kernel/interrupt.h Qiusuo/kernel/memory.h 		\
	Qiusuo/lib/stdio.h Qiusuo/lib/string.h 
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/stdio-kernel.o: Qiusuo/lib/kernel/stdio-kernel.c Qiusuo/lib/kernel/stdio-kernel.h Qiusuo/kernel/global.h Qiusuo/lib/stdio.h Qiusuo/device/console.h \
	Qiusuo/lib/stdint.h 
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/fs.o: Qiusuo/fs/fs.c Qiusuo/fs/fs.h Qiusuo/fs/dir.h Qiusuo/fs/inode.h Qiusuo/fs/super_block.h Qiusuo/kernel/global.h		\
	Qiusuo/lib/stdint.h Qiusuo/lib/kernel/stdio-kernel.h Qiusuo/kernel/memory.h Qiusuo/kernel/debug.h Qiusuo/lib/string.h \
	Qiusuo/lib/kernel/list.h Qiusuo/device/console.h
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/dir.o: Qiusuo/fs/dir.c Qiusuo/fs/dir.h Qiusuo/fs/inode.h Qiusuo/fs/super_block.h Qiusuo/kernel/global.h Qiusuo/fs/fs.h		\
	Qiusuo/lib/stdint.h Qiusuo/lib/kernel/stdio-kernel.h  Qiusuo/kernel/debug.h Qiusuo/lib/string.h Qiusuo/fs/file.h Qiusuo/device/ide.h		
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/file.o: Qiusuo/fs/file.c  Qiusuo/fs/file.h Qiusuo/fs/inode.h Qiusuo/fs/super_block.h Qiusuo/kernel/global.h Qiusuo/fs/fs.h		\
	Qiusuo/lib/stdint.h Qiusuo/lib/kernel/stdio-kernel.h  Qiusuo/kernel/debug.h Qiusuo/lib/string.h Qiusuo/device/ide.h	Qiusuo/fs/dir.h		\
	Qiusuo/thread/thread.h Qiusuo/lib/kernel/bitmap.h 
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/inode.o: Qiusuo/fs/inode.c Qiusuo/fs/inode.h Qiusuo/fs/super_block.h Qiusuo/kernel/global.h Qiusuo/fs/fs.h	Qiusuo/kernel/debug.h 	\
	Qiusuo/lib/stdint.h Qiusuo/lib/kernel/stdio-kernel.h  Qiusuo/kernel/debug.h Qiusuo/lib/string.h Qiusuo/fs/file.h Qiusuo/device/ide.h		\
	Qiusuo/thread/thread.h Qiusuo/kernel/interrupt.h 
	$(CC) $(CFLAGS) -o $@ $<

########################         汇编代码编译        ########################
########        boot        #######
$(BUILD_DIR)/mbr.bin: Qiusuo/boot/mbr.s Qiusuo/boot/include/boot.inc
	nasm -I  Qiusuo/boot/include/ -o $@ $<
  
$(BUILD_DIR)/loader.bin: Qiusuo/boot/loader.s Qiusuo/boot/include/boot.inc
	nasm -I  Qiusuo/boot/include/ -o $@ $<

########        other asm        ########
$(BUILD_DIR)/kernel.o: Qiusuo/kernel/kernel.s
	$(AS) $(ASFLAGS) -o $@ $<

$(BUILD_DIR)/print.o: Qiusuo/lib/kernel/print.s 
	$(AS) $(ASFLAGS) -o $@ $<

$(BUILD_DIR)/switch.o: Qiusuo/thread/switch.s 
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
	dd if=$(BUILD_DIR)/kernel.bin of=$(HD60M_PATH) bs=512 count=255 seek=9 conv=notrunc

clean:
	cd $(BUILD_DIR) && rm -f ./*

all: mk_dir boot build hd 

reall: clean all
