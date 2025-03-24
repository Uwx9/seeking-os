BUILD_DIR = ./build
ENTRY_POINT = 0xc0001500

CC = gcc
AS = nasm
LD = ld 

LIB = -I lib -I lib/kernel -I lib/usr -I device/ -I kernel/ 

ASFLAGS = -f elf
CFLAGS = -c -m32 -Wall -fno-stack-protector $(LIB) -fno-builtin -W -Wstrict-prototypes -Wmissing-prototypes
LDFLAGS = -m elf_i386 -Ttext $(ENTRY_POINT) -e main -Map $(BUILD_DIR)/kernel.map

OBJS = $(BUILD_DIR)/main.o 		$(BUILD_DIR)/init.o 	$(BUILD_DIR)/interrupt.o	\
	   $(BUILD_DIR)/timer.o		$(BUILD_DIR)/kernel.o	$(BUILD_DIR)/print.o 	 	\
	   $(BUILD_DIR)/debug.o		$(BUILD_DIR)/string.o 	$(BUILD_DIR)/bitmap.o 		\
	   $(BUILD_DIR)/memory.o 
	   
########################         C代码编译        ########################
$(BUILD_DIR)/main.o: kernel/main.c kernel/debug.h kernel/init.h		\
	lib/stdint.h lib/kernel/print.h	
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/init.o: kernel/init.c kernel/init.h kernel/interrupt.h device/timer.h		\
	lib/stdint.h
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/interrupt.o: kernel/interrupt.c kernel/interrupt.h kernel/global.h		\
	lib/stdint.h lib/kernel/io.h lib/kernel/print.h
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/timer.o: device/timer.c device/timer.h		\
	lib/kernel/print.h lib/kernel/io.h lib/stdint.h
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/debug.o: kernel/debug.c kernel/debug.h kernel/interrupt.h		\
	lib/kernel/print.h lib/stdint.h
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/string.o: lib/string.c lib/string.h kernel/debug.h kernel/global.h		\
	lib/stdint.h 
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/bitmap.o: lib/kernel/bitmap.c lib/kernel/bitmap.h lib/stdint.h lib/string.h 	\
	kernel/interrupt.h kernel/debug.h kernel/global.h 
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/memory.o: kernel/memory.c kernel/memory.h kernel/debug.h kernel/global.h	\
	lib/stdint.h lib/kernel/print.h lib/kernel/bitmap.h lib/string.h 
	$(CC) $(CFLAGS) -o $@ $<



########################         汇编代码编译        ########################
$(BUILD_DIR)/mbr.bin: boot/mbr.s 
	nasm -I boot/include/ -o $@ $<
  
$(BUILD_DIR)/loader.bin: boot/loader.s 
	nasm -I boot/include/ -o $@ $<

$(BUILD_DIR)/kernel.o: kernel/kernel.s
	$(AS) $(ASFLAGS) -o $@ $<

$(BUILD_DIR)/print.o: lib/kernel/print.s 
	$(AS) $(ASFLAGS) -o $@ $<

########################         链接所有目标文件        ########################
$(BUILD_DIR)/kernel.bin: $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

.PHONY: mk_dir all clean hd

mk_dir:
	if [ [! -d $(BUILD_DIR)] ]; then mkdir $(BUILD_DIR); fi

hd:
	dd if=$(BUILD_DIR)/mbr.bin of=../bochs/hd60M.img bs=512 count=1 conv=notrunc 
	dd if=$(BUILD_DIR)/loader.bin of=../bochs/hd60M.img bs=512 count=4 seek=2 conv=notrunc
	dd if=$(BUILD_DIR)/kernel.bin of=../bochs/hd60M.img bs=512 count=200 seek=9 conv=notrunc

clean:
	cd $(BUILD_DIR) && rm -f ./*

build: $(BUILD_DIR)/kernel.bin

boot: $(BUILD_DIR)/mbr.bin $(BUILD_DIR)/loader.bin

all: mk_dir boot build hd 

#警告一大堆，得仔细研究一下，关于编译原理这一块真是复杂
