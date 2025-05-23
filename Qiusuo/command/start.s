[bits 32]

extern main
extern exit

section .text
global _start
_start:
	; 在execv中将argv放在中断栈ebx中，argc放在中断栈ecx中然后从中断返回为寄存器赋值并跳转
	push ebx	; 压入argv
	push ecx	; 压入argc
	call main

	;将 main的返回值通过栈传给exit，gcc用eax存储返回值，这是ABI规定的
	push eax
	call exit 	;exit 不会返回, 调用exit后进程体被回收，父进程回收子进程pcb
