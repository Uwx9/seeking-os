BUILD_DIR = ./Qiusuo/build
ENTRY_POINT = 0xc0001500
HD_PATH = ../bochs/hd60M.img

CC = gcc
AS = nasm
LD = ld 

LIB = -I  Qiusuo/lib -I  Qiusuo/lib/kernel -I  Qiusuo/lib/usr -I  Qiusuo/device/ -I Qiusuo/kernel/ 

ASFLAGS = -f elf
CFLAGS = -c -m32 -Wall -fno-stack-protector $(LIB) -fno-builtin -W -Wstrict-prototypes -Wmissing-prototypes
LDFLAGS = -m elf_i386 -Ttext $(ENTRY_POINT) -e main -Map $(BUILD_DIR)/kernel.map

OBJS = $(BUILD_DIR)/main.o 		$(BUILD_DIR)/init.o 	$(BUILD_DIR)/interrupt.o	\
	   $(BUILD_DIR)/timer.o		$(BUILD_DIR)/kernel.o	$(BUILD_DIR)/print.o 	 	\
	   $(BUILD_DIR)/debug.o		$(BUILD_DIR)/string.o 	$(BUILD_DIR)/bitmap.o 		\
	   $(BUILD_DIR)/memory.o 
	   
########################         C代码编译        ########################
$(BUILD_DIR)/main.o: Qiusuo/kernel/main.c Qiusuo/kernel/debug.h Qiusuo/kernel/init.h		\
	 Qiusuo/lib/stdint.h  Qiusuo/lib/kernel/print.h	
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/init.o: Qiusuo/kernel/init.c Qiusuo/kernel/init.h Qiusuo/kernel/interrupt.h  Qiusuo/device/timer.h		\
	 Qiusuo/lib/stdint.h
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/interrupt.o: Qiusuo/kernel/interrupt.c Qiusuo/kernel/interrupt.h Qiusuo/kernel/global.h		\
	 Qiusuo/lib/stdint.h  Qiusuo/lib/kernel/io.h  Qiusuo/lib/kernel/print.h
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/timer.o:  Qiusuo/device/timer.c  Qiusuo/device/timer.h		\
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
	 Qiusuo/lib/stdint.h  Qiusuo/lib/kernel/print.h  Qiusuo/lib/kernel/bitmap.h  Qiusuo/lib/string.h 
	$(CC) $(CFLAGS) -o $@ $<



########################         汇编代码编译        ########################
$(BUILD_DIR)/mbr.bin:  Qiusuo/boot/mbr.s 
	nasm -I  Qiusuo/boot/include/ -o $@ $<
  
$(BUILD_DIR)/loader.bin:  Qiusuo/boot/loader.s 
	nasm -I  Qiusuo/boot/include/ -o $@ $<

$(BUILD_DIR)/kernel.o: Qiusuo/kernel/kernel.s
	$(AS) $(ASFLAGS) -o $@ $<

$(BUILD_DIR)/print.o:  Qiusuo/lib/kernel/print.s 
	$(AS) $(ASFLAGS) -o $@ $<

########################         链接所有目标文件        ########################
$(BUILD_DIR)/kernel.bin: $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

.PHONY: mk_dir all clean hd

mk_dir:
	if [ ! -d "$(BUILD_DIR)" ]; then mkdir -p "$(BUILD_DIR)"; fi
hd:
	dd if=$(BUILD_DIR)/mbr.bin of=$(HD_PATH) bs=512 count=1 conv=notrunc 
	dd if=$(BUILD_DIR)/loader.bin of=$(HD_PATH) bs=512 count=4 seek=2 conv=notrunc
	dd if=$(BUILD_DIR)/kernel.bin of=$(HD_PATH) bs=512 count=200 seek=9 conv=notrunc

clean:
	cd $(BUILD_DIR) && rm -f ./*

build: mk_dir $(BUILD_DIR)/kernel.bin

mkboot: mk_dir $(BUILD_DIR)/mbr.bin $(BUILD_DIR)/loader.bin

all: mk_dir mkboot build hd 

#警告一大堆，得仔细研究一下，关于编译原理这一块真是复杂
