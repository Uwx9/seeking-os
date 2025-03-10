TI_GDT equ 0
RPL0 equ 0
SELECTOR_VIDEO equ (0x0003<<3) + TI_GDT + RPL0

[bits 32]
section .data
	put_int_buffer dq 0
section .text
;-------------------------------------------------------------------- 
;put_str 通过 put_char 来打印以0字符结尾的字符串 
;--------------------------------------------------------------------;
global put_str
put_str:
	push ebx
	push ecx
	xor ecx,ecx
	mov ebx,[esp+12]		;ebx为传入的参数，字符串首地址
.goon:	
	mov cl,[ebx]
	cmp cl,0
	jz .str_over
	push ecx
	call put_char
	add esp,4
	inc ebx
	jmp .goon
.str_over:
	pop	ecx
	pop ebx
	ret

;------------------------   put_char   ----------------------------- 
;功能描述：把栈中的1个字符写入光标所在处 
;------------------------------------------------------------------- 
global put_char
put_char:
    pushad          		;备份32位寄存器入栈先后顺序是：EAX->ECX->EDX->EBX-> ESP-> EBP->ESI->EDI
	mov ax,SELECTOR_VIDEO
	mov gs,ax

;获取当前光标位置
	mov dx,0x3d4    		;索引寄存器 
    mov al,0x0e
    out dx,al
    mov dx,0x3d5
    in al,dx       		 	;通过读写数据端口0x3d5来获得或设置光标位置
    mov ah,al       		;高8位

    ;得到低8位
    mov dx,0x3d4
    mov al,0x0f
    out dx,al
    mov dx,0x3d5
    in al,dx

    mov bx,ax                   ;将光标存入bx
    mov ecx,[esp+36]            ;跳过8个寄存器和函数返回地址

    cmp cl,0xd                  ;CR是0x0d，LF是0x0a
    jz .is_carriage_return 
    cmp cl,0xa
    jz .is_line_feed
    cmp cl,0x8                  ;BS(backspace)的asc码是8
    jz .is_backspace

    jmp .put_other

.is_backspace:
    dec bx
    shl bx,1
    mov byte [gs:bx],0x20		;将待删除的字节补为0或空格皆可
    inc bx
    mov byte [gs:bx],0x07
    shr bx,1                    ;恢复bx到退格后的位置
    jmp .set_cursor
	
.put_other:
	shl bx,1
	mov byte [gs:bx],cl			;写入字符asc码
	inc bx
	mov byte [gs:bx],0x7
	shr bx,1
	inc bx						;下一个光标值

	cmp bx,2000
	jl .set_cursor				;光标值小于2000表示未写到显存的最后，若超出屏幕字符数大小则换行处理 														

.is_line_feed:					;是换行符LF(\n)
.is_carriage_return:			;是回车符CR(\r)
	mov si,80
	xor dx,dx
	mov ax,bx
	div si
	sub bx,dx					;光标值减去余数，光标回到行首

.is_carriage_return_end:
	add bx,80
	cmp bx,2000
.is_line_feed_end:
	jl .set_cursor	

;（1）将第1～24行的内容整块搬到第0～23行，也就是把第0行的数据覆盖。 
;（2）再将第24行，也就是最后一行的字符用空格覆盖，这样它看上去是一个新的空行。 
;（3）把光标移到第24行也就是最后一行行首。
.roll_screen:
	cld
	mov ecx,960					;一共24行，80*24=1920字符，960个双字
	mov esi,0xc00b80a0			;第一行
	mov edi,0xc00b8000			;第0行
	rep movsd

;将最后一行填充为空白	
	mov ebx,3840				;最后一行第一个字节,为什么不用bx，因为bx仅仅代表光标值，不要破坏它
	mov ecx,80
.cls:
	mov word [gs:ebx],0x0720	;0x0720是黑底白字的空格键
	add ebx,2
	loop .cls
	
	mov bx,1920

.set_cursor:
;先设置高8位
	mov dx,0x3d4
	mov al,0x0e
	out dx,al
	mov dx,0x3d5
	mov al,bh
	out dx,al
;设置低8位
	mov dx,0x3d4
	mov al,0x0f
	out dx,al
	mov dx,0x3d5
	mov al,bl
	out dx,al

.put_char_done:
	popad
	ret

global put_int
put_int:
	pushad
	mov ebp,esp
	mov eax,[ebp+4*9]			;拿到参数
	mov edx,eax
	mov edi,7					;buffer最高位
	mov ebx,put_int_buffer
	mov ecx,8

.16based_4bits:
	and edx,0x0000000f			;一次处理4位
	cmp edx,9
	jg .is_A2F
	add edx,'0'
	jmp .store					;store存储

.is_A2F:
	sub edx,10
	add edx,'a'

.store:
	mov [ebx+edi],dl			;参数低位的asci码放到buffer高位，便于显示	
	dec edi
	shr eax,4					;已经处理4位，更新
	mov edx,eax
	loop .16based_4bits

;把高位连续的字符去掉，比如把字符000123变成123
;edi是第一个非0字符的偏移
.ready_to_print:
	inc edi							;此时edi退减为-1(0xffffffff)，加1使其为0
.skip_prefix_0:	
	cmp edi,8
	je .full0
.go_on_skip:
	mov cl,[put_int_buffer+edi]		;希望拿到第一个非0字符	
	inc edi
	cmp cl,'0'
	je .skip_prefix_0
	dec edi							;第一个非0字符的偏移
	jmp .put_each_number

.full0:
	mov cl,'0'

.put_each_number:
	push ecx					 	
	call put_char
	add esp,4
	inc edi
	mov cl,[put_int_buffer+edi]
	cmp edi,7
	jle .put_each_number
	popad
	ret
