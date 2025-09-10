%include "boot.inc"
section loader vstart=LOADER_BASE_ADDR
LOADER_STACK_TOP equ LOADER_BASE_ADDR

;构建GDT及其内部的描述符
GDT_BASE:	dd 0x00000000		;第0个描述符不用
			dd 0x00000000

CODE_DESC:	dd 0x0000ffff
			dd DESC_CODE_HIGH4

DATA_DESC:	dd 0x0000ffff
			dd DESC_DATA_HIGH4

VIDEO_DESC:	dd 0x80000007		;段基址0~15是8000，limit是7*4k
			dd DESC_VIDEO_HIGH4
		
GDT_SIZE equ $ - GDT_BASE
GDT_LIMIT equ GDT_SIZE - 1

SELECTOR_CODE equ (0x0001<<3) + TI_GDT + RPL0
SELECTOR_DATA equ (0x0002<<3) + TI_GDT + RPL0
SELECTOR_VIDEO equ (0x0003<<3) + TI_GDT + RPL0

times 60 dq 0				;预留60个描述符的空位，四字

;total_mem_bytes用于保存内存容量，当前偏移文件头0x200位置
total_mem_bytes dd 0		;内存中位置为0x900+0x200=0xb00

;gdt指针前两字节为gdt界限最大为2的16次方减一，后四字节为gdt起始地址
gdt_ptr	dw GDT_LIMIT		;更类似变量
		dd GDT_BASE			;仅地址标号

;人工对齐，total_mem_bytes+gdt_ptr+ards_buf+ards_nr，共256个字节
ards_buf times 244 db 0
ards_nr dw 0				;用于记录ards结构体数量

loader_start:
;int 0x15	eax=0000e820h, edx=534d4150h('SMAP')获取内存布局

	xor ebx,ebx						;第一次调用时ebx值要为0
	mov edx,0x534d4150				;edx只用赋值一次，循环中不会改变
	mov di,ards_buf					;ards结构缓冲区
.e820_mem_get_loop:
	mov eax,0x0000e820				;每次执行后返回eax变为0x534d4150，需要重置
	mov ecx,20						;ARDS结构的字节大小：用来指示BIOS写入的字节数
	int 0x15
	jc .e820_failed_so_try_e801		;cf返回1表示发生错误，调用0xe801子功能号
	add di,cx						;di指向新的ards结构体地址
	inc word [ards_nr]				;ards个数加一
	cmp ebx,0						;若cf=0且ebx为0表示这是最后一个ards
	jnz .e820_mem_get_loop

;在所有的ards中找到(base_add_low+lengh_low)的最大值
	mov cx,[ards_nr]
	mov ebx,ards_buf
	xor edx,edx			;最大内存容量

.find_max_mem_area:
	mov eax,[ebx]		;base_add_low		
	add eax,[ebx+8] 	;lenth_low
	add ebx,20
	cmp edx,eax
	jge .next_ards	
	mov edx,eax

.next_ards:
	loop .find_max_mem_area
	jmp .mem_get_ok

;------------------ int 15h ax = E801h 获取内存大小，最大支持4G -------------------- 
; 返回后, ax cx 值一样,以KB为单位，bx dx值一样，以64KB为单位 
; 在 ax 和cx寄存器中为低16MB，在bx和dx寄存器中为16MB到4GB 	
.e820_failed_so_try_e801:
	mov eax,0xe801
	int 0x15
	jc .e801_failed_so_try_88

;1 先算出低15MB的内存 
;ax和cx中是以KB为单位的内存数量，将其转换为以byte为单位
	mov cx,0x400
	mul cx
	;把低15M内存大小放到edx中
	shl edx,16
	and eax,0x0000ffff
	or edx,eax

	add eax,0x100000		;还要再加1M
	mov esi,eax				;备份低15M内存

;2 再将 16MB以上的内存转换为byte为单位 
;寄存器bx和dx中是以64KB为单位的内存数量 
	xor eax,eax
	mov ax,bx
	mov ecx,0x10000
	mul ecx					;64位的积，低32位的eax足够了,此方法只能测出4GB以内的内存,edx=0
	add esi,eax
	mov edx,esi
	jmp .mem_get_ok

;-----  int 15h ah = 0x88 获取内存大小，只能获取64MB之内  -----
.e801_failed_so_try_88:
	mov ah,0x88
	int 0x15
	jc .error_hlt
	and eax,0x0000ffff
	mov cx,0x400
	mul cx
	shl edx,16
	or edx,eax
	add edx,0x100000			;再加1M

.mem_get_ok:
	mov [total_mem_bytes],edx	;将内存换算成byte单位放到[total_mem_bytes]
	
;---------------- 准备进入保护模式 ----------------
;1打开A20
;2加载gdt
;3将cr0的pe位置为1

;打开A20
	in al,0x92
	or al,0000_0010
	out 0x92,al

;加载gdt
	lgdt [gdt_ptr]

;cr0第0位置1
	mov eax,cr0 
	or eax,0x00000001
	mov cr0,eax

;-------------------- 已经进入保护模式 -------------------
	jmp dword SELECTOR_CODE:p_mode_start	;刷新流水线

.error_hlt:						
 	hlt							;暂停 CPU 的执行,进入一种低功耗的等待直到接收到外部中断或系统管理中断


[bits 32]
p_mode_start:
	mov ax,SELECTOR_DATA
	mov ds,ax
	mov es,ax
	mov ss,ax
	mov esp,LOADER_STACK_TOP

; -------------------------   加载 kernel  ---------------------- 
	mov eax, KERNEL_START_SECTOR  		;kernel.bin 所在的扇区号 
    mov ebx, KERNEL_BIN_BASE_ADDR 		;从磁盘读出后，写入到 ebx 指定的地址 
    mov ecx, 370                   		;读入的扇区数
	call rd_disk_m_32

;---------------------- 启动分页 ---------------------
	call setup_page
	sgdt [gdt_ptr]						;将gdtr中的值放入[gdt_ptr]中


	mov ebx,[gdt_ptr+2]					;将gdt描述符中视频段描述符中的段基址+0xc0000000 
	or dword [ebx+0x18+4],0xc0000000

	add dword [gdt_ptr+2],0xc0000000	;将gdt的基址加上0xc0000000使其成为内核所在的高地址


	add esp,0xc0000000					;将栈指针同样映射到内核地址

;把页目录地址赋给cr3 
	mov eax,PAGE_DIR_TABLE_POS
	mov cr3,eax

;打开cr0的pg位（第31位）
	mov eax,cr0
	or eax,0x80000000
	mov cr0,eax

;在开启分页后，用gdt的新地址重新加载 
	lgdt [gdt_ptr]

;---------------------- 初始化内核 ---------------------
	call kernel_init
	mov esp,0xc009f000
	jmp KERNEL_ENTRY_POINT

;----------   将kernel.bin中的segment拷贝到编译的地址   ----------- 
kernel_init:
	xor eax,eax
	xor ebx,ebx		;ebx记录程序头表地址	
	xor ecx,ecx		;cx记录程序头表中program header的数量
	xor edx,edx		;dx记录program header尺寸，即e_phentsize

	mov dx,[KERNEL_BIN_BASE_ADDR + 42]		;偏移elf_header42个字节处是e_phentsize
	mov ebx,[KERNEL_BIN_BASE_ADDR + 28]		;e_phoff,表示第 1 个 program header 在文件中的偏移量 
	add ebx,KERNEL_BIN_BASE_ADDR
	mov cx,[KERNEL_BIN_BASE_ADDR + 44]

.each_segment:
	cmp dword [ebx], PT_NULL		;比较程序头表p_type,0表示忽略
	je .PTNULL

;为函数memcpy压入参数，参数是从右往左依然压入,函数原型类似于 memcpy（dst，src，size）
	push dword [ebx + 16]			;压入p_filesz
	mov eax,[ebx + 4]
	add eax,KERNEL_BIN_BASE_ADDR
	push eax						;压入第一个段要放在内存中的地址,源地址
	push dword [ebx + 8]			;压入p_vaddr目的地址
	 
	call mem_cpy
	add esp,12

.PTNULL:
	add ebx,edx						;指向下一个程序头
	loop .each_segment
	ret

;----------  逐字节拷贝 mem_cpy（dst，src，size） ------------ 
;输入:栈中三个参数（dst，src，size） 
;输出:无 
;--------------------------------------------------------- 
mem_cpy:
	cld								;esi，edi递增
	push ebp
	mov ebp,esp
	push ecx
	mov edi,[ebp + 8]
	mov esi,[ebp + 12]
	mov ecx,[ebp + 16]
	rep movsb
	pop ecx
	pop ebp
	ret
	
;------------- 创建页目录及页表 --------------- 
;把1M上面4k的内存清空给页目录用
setup_page:
	mov ecx,0x1000
	mov esi,0
.clear_page_dir:
	mov byte [PAGE_DIR_TABLE_POS+esi],0
	inc esi
	loop .clear_page_dir

;开始创建页目录项(PDE) 
.create_pde:								;创建Page Directory Entry
	mov eax,PAGE_DIR_TABLE_POS
	add eax,0x1000							;eax指向第一个页表的位置
	mov ebx,eax								;第一个页表的位置将来会用到要保存下来
	or eax,PAGE_US_U | PAGE_RW_W | PAGE_P   ;逻辑或选择属性组合
	mov [PAGE_DIR_TABLE_POS+0x0],eax		;页目录的第0项和第768项都放第一个页表的地址+属性
	mov [PAGE_DIR_TABLE_POS+0xc00],eax		;0xc00代表页目录的3/4处，上面的1G属于os
	sub eax,0x1000
	mov [PAGE_DIR_TABLE_POS+4092],eax		;页目录最后一项放页目录自身的地址

;下面创建页表项(PTE)
	mov ecx,256								;把第一个页表的前256项(可映射1M)映射到物理地址低1M
	mov esi,0
	mov edx,PAGE_US_U | PAGE_RW_W | PAGE_P

.create_pte:
	mov [ebx+esi*4],edx
	add edx,0x1000
	inc esi
	loop .create_pte

;创建内核其他页表的PDE,但还没有与物理地址映射，只是填充了页目录项
	mov eax,PAGE_DIR_TABLE_POS
	add eax,0x2000							;eax为第二个页表的位置
	or eax,PAGE_US_U | PAGE_RW_W | PAGE_P
	mov ebx,PAGE_DIR_TABLE_POS

;页目录第0项和第768项已经创建，现将第769~第1022项的虚拟地址映射到低地址
;第768项到第1022项似乎不够虚拟地址3G—4G差了4M，第1023项存PDE地址了，但感觉问题不大
	mov ecx,254		
	mov esi,769

.create_kernel_pde:
	mov [ebx+esi*4],eax
	inc esi
	add eax,0x1000
	loop .create_kernel_pde
	ret


rd_disk_m_32:
;-----------------------------------------------------------
; 功能：从硬盘读取多个扇区（支持 >255 扇区）
; 输入：
;   - eax = LBA 起始扇区号
;   - ebx = 数据存储的内存地址
;   - ecx = 要读取的扇区数（支持 >255）
; 说明：
;   - 使用栈保存进度（LBA/内存地址/剩余扇区数）
;   - 用 esi 临时保存本次读取的扇区数
;-----------------------------------------------------------
    push eax            ; 原始LBA [esp+8]
    push ebx            ; 内存地址 [esp+4]
    push ecx            ; 剩余扇区数 [esp]

.read_block:
    ; 检查是否完成
    mov ecx, [esp]      ; 剩余扇区数
    cmp ecx, 0
    jle .done

    ; 计算本次读取量（≤255扇区）
    cmp ecx, 255
    jbe .set_count
    mov ecx, 255
.set_count:
    mov esi, ecx        ; esi = 本次读取扇区数

    ; 设置读取扇区数（0x1f2）
    mov dx, 0x1f2
    mov al, cl
    out dx, al

    ; 设置LBA地址（从栈获取当前LBA）
    mov eax, [esp+8]    ; 获取当前LBA

    ; LBA 0-7 → 0x1f3
    mov dx, 0x1f3
    out dx, al

    ; LBA 8-15 → 0x1f4
    shr eax, 8
    mov dx, 0x1f4
    out dx, al

    ; LBA 16-23 → 0x1f5
    shr eax, 8
    mov dx, 0x1f5
    out dx, al

    ; LBA 24-27 → 0x1f6（主盘 + LBA模式）
    shr eax, 8
    and al, 0x0F
    or al, 0xE0
    mov dx, 0x1f6
    out dx, al

    ; 发送读命令（0x20）
    mov dx, 0x1f7
    mov al, 0x20
    out dx, al

    ; 等待硬盘就绪
.wait_ready:
    in al, dx
    and al, 0x88
    cmp al, 0x08
    jnz .wait_ready

    ; 读取数据（esi*256 words）
    mov ecx, esi        ; 本次读取扇区数
    shl ecx, 8          ; 每扇区256 words
    mov dx, 0x1f0
    mov edi, [esp+4]    ; 目标内存地址
.read_loop:
    in ax, dx
    mov [edi], ax
    add edi, 2
    loop .read_loop

    ; 更新状态：
    ; 1. 内存地址 += 扇区数*512
    mov eax, esi        ; 本次读取扇区数
    shl eax, 9          ; *512
    add [esp+4], eax    ; 更新内存地址

    ; 2. LBA += 扇区数
    mov eax, esi        ; 本次读取扇区数
    add [esp+8], eax    ; 更新LBA

    ; 3. 剩余扇区数 -= 本次读取量
    sub [esp], eax      ; 更新剩余扇区数

    jmp .read_block

.done:
    add esp, 12         ; 清理栈
    ret