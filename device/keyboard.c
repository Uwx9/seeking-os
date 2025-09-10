#include "keyboard.h"
#include "io.h"
#include "interrupt.h"
#include "print.h"
#include "global.h"
#include "ioqueue.h"

#define KBD_BUF_PORT 0x60		//键盘 buffer 寄存器端口号为0x60

/* 用转义字符定义部分控制字符 */
#define esc '\033'				//八进制表示字符
#define backspace '\b'
#define tab '\t'
#define enter '\r'
#define delete '\0177'

/* 以上不可见字符一律定义为0 */
#define char_invisible 0
#define ctrl_l_char char_invisible
#define ctrl_r_char char_invisible
#define shift_l_char char_invisible
#define shift_r_char char_invisible
#define alt_l_char char_invisible
#define alt_r_char char_invisible
#define caps_lock_char char_invisible

/* 定义控制字符的通码和断码 */
#define ctrl_l_make 0x1d 
#define ctrl_r_make 0xe01d
#define ctrl_r_break 0xe09d
#define shift_l_make 0x2a
#define shift_r_make 0x36
#define alt_l_make 0x38
#define alt_r_make 0xe038
#define alt_r_break 0xe0b8
#define caps_lock_make 0x3a

struct ioqueue kbd_buf;		//键盘缓冲区 

/* 以通码make_code为索引的二维数组 */
static char keymap[][2] = {
/* 第0个元素是某个按键未与shift键组合时对应的字符ASCII码值，第1个元素是某个按键与shift键组合时对应的字符ASCII码值
 * 可以说是扫描码到asci码的映射 */
/* 下标 */    /* 扫描码未与或与shift组合*/
/* 0x00 */		{0, 0},
/* 0x01 */		{esc, esc},
/* 0x02 */		{'1', '!'},
/* 0x03 */		{'2', '@'},
/* 0x04 */		{'3', '#'},
/* 0x05 */		{'4', '$'},
/* 0x06 */		{'5', '%'},
/* 0x07 */		{'6', '^'},
/* 0x08 */		{'7', '&'},
/* 0x09 */		{'8', '*'},
/* 0x0a */		{'9', '('},
/* 0x0b */		{'0', ')'},
/* 0x0c */		{'-', '_'},
/* 0x0d */		{'=', '+'},
/* 0x0e */		{backspace, backspace},
/* 0x0f */		{tab, tab},
/* 0x10 */		{'q', 'Q'},
/* 0x11 */		{'w', 'W'},
/* 0x12 */		{'e', 'E'},
/* 0x13 */		{'r', 'R'},
/* 0x14 */		{'t', 'T'},
/* 0x15 */		{'y', 'Y'},
/* 0x16 */		{'u', 'U'},
/* 0x17 */      {'i',   'I'}, 
/* 0x18 */      {'o',   'O'}, 
/* 0x19 */      {'p',   'P'}, 
/* 0x1A */      {'[',   '{'}, 
/* 0x1B */      {']',   '}'}, 
/* 0x1C */      {enter,  enter}, 
/* 0x1D */      {ctrl_l_char, ctrl_l_char}, 
/* 0x1E */      {'a',   'A'}, 
/* 0x1F */      {'s',   'S'}, 
/* 0x20 */      {'d',   'D'}, 
/* 0x21 */      {'f',   'F'}, 
/* 0x22 */      {'g',   'G'}, 
/* 0x23 */      {'h',   'H'}, 
/* 0x24 */      {'j',   'J'}, 
/* 0x25 */      {'k',   'K'}, 
/* 0x26 */      {'l',   'L'}, 
/* 0x27 */      {';',   ':'}, 
/* 0x28 */      {'\'',  '"'}, 
/* 0x29 */      {'`',   '~'}, 
/* 0x2A */      {shift_l_char, shift_l_char}, 
/* 0x2B */      {'\\',  '|'}, 
/* 0x2C */      {'z',   'Z'}, 
/* 0x2D */      {'x',   'X'}, 
/* 0x2E */      {'c',   'C'}, 
/* 0x2F */      {'v',   'V'}, 
/* 0x30 */      {'b',   'B'}, 
/* 0x31 */      {'n',   'N'}, 
/* 0x32 */      {'m',   'M'}, 
/* 0x33 */      {',',   '<'}, 
/* 0x34 */      {'.',   '>'}, 
/* 0x35 */      {'/',   '?'}, 
/* 0x36 */      {shift_r_char, shift_r_char}, 
/* 0x37 */      {'*',   '*'}, 
/* 0x38 */      {alt_l_char, alt_l_char}, 
/* 0x39 */      {' ',   ' '}, 
/* 0x3A */      {caps_lock_char, caps_lock_char}
/* 很妙啊，这个通码就是按键盘上的顺序来排的，方便处理 */
/*其他按键暂不处理*/
};

/* 定义以下变量记录相应键是否按下的状态，ext_scancode用于记录makecode是否以0xe0开头 */
static bool ctrl_status, shift_status, alt_status, caps_lock_status, ext_scancode;

/* 键盘中断处理程序 */
/* static限制的是链接器对其他文件可见性，但在本文件内完全可见。中断发生时，CPU直接跳转到存储的地址执行，不经过任何名称查找过程。 */
static void intr_keyboard_handler(void)
{
	/* 这次中断发生前的上一次中断，以下任意三个键是否有按下 */
	bool ctrl_down_last = ctrl_status;
	bool shift_down_last = shift_status;
	bool caps_lock_last = caps_lock_status;
	bool break_code;
	uint16_t scancode = inb(KBD_BUF_PORT);

/* 若扫描码scancode是e0开头的，表示此键的按下将产生多个扫描码，所以马上结束此次中断处理函数，等待下一个扫描码进来 */
	if (scancode == 0xe0) {
		ext_scancode = true;	//打开e0标记
		return;
	}

/* 如果上次是以0xe0开头的，将扫描码合并 */
	if (ext_scancode) {
		scancode = 0xe000 | scancode;
		ext_scancode = false;	//关闭e0标记
	} 

	/* 获取break_code,如果是断码，加了0x80后第7位为1，&0x80后结果不为0，如果是通码&上后为0 */ 
	break_code = ((scancode & 0x0080) != 0);

	if (break_code) {	//若是断码break_code（按键弹起时产生的扫描码）
	/* 由于 ctrl_r 和alt_r的make_code和break_code都是两字节，所以可用下面的方法取make_code，多字节的扫描码暂不处理 */
	uint16_t make_code = scancode & 0xff7f;
	/* 若是任意以下三个键弹起了，将状态置为false */
	if (make_code == ctrl_l_make || make_code == ctrl_r_make) {
		ctrl_status = false;
	} else if (make_code == shift_l_make || make_code == shift_r_make) {
		shift_status = false;
	} else if (make_code == alt_l_make || make_code == alt_r_make) {
		alt_status = false;
	}
	/* 由于 caps_lock 不是弹起后关闭，所以需要单独处理 */
	return;
	}
/* 若为通码，只处理数组中定义的键以及alt_right和ctrl键(没有在数组中，因为它们扫描码是两字节，不好开那么大的数组)，全是make_code */
	else if ((scancode > 0x00 && scancode < 0x3b) || (scancode == alt_r_make) || (scancode == ctrl_r_make)) {
		bool shift = false;		//判断是否与shift组合，用来在一维数组中索引对应的字符
		//非字母只用在意shift 
		if ( (scancode < 0x0e) || (scancode == 0x29) || (scancode == 0x1a) || (scancode == 0x1b) || (scancode == 0x34) || \
		    (scancode == 0x2b) || (scancode == 0x27) || (scancode == 0x28) || (scancode == 0x33) || (scancode == 0x35)) {
				/****** 代表双字符的键 ******** 
                0x0e 数字'0'～'9',字符'-',字符'=' 
                            0x29 字符'`' 
                            0x1a 字符'[' 
                            0x1b 字符']' 
                            0x2b 字符'\\' 
                            0x27 字符';' 
                            0x28 字符'\'' 
                            0x33 字符',' 
                            0x34 字符'.' 
                            0x35 字符'/' 
                *******************************/
				if (shift_down_last) {
					shift = true;
				}
			} else {	//默认为字母键还要看caps_lock
				if (shift_down_last && caps_lock_last) {
					shift = false;
				} else if (shift_down_last || caps_lock_last) {
					shift = true;
				} else {
					shift = false;
				}
			}
			/* 将扫描码的高字节置0，主要针对高字节是e0的扫描码 */
			uint8_t index = (scancode & 0x00ff);

			//在数组中找到对应的字符
			char cur_char = keymap[index][shift];		

			// ctrl+l和ctrl+u分别为清屏和删除输入的快捷键，让它们变为不可见字符再处理，当然不能让其值等于'\b''\n'等有用的字符
			if ((ctrl_down_last && cur_char == 'l') || (ctrl_down_last && cur_char == 'u')) {
				cur_char -= 'a';
			}

			/* 只处理 ASCII码不为0的键 */
			if (cur_char) {
			/* 若 kbd_buf 中未满并且待加入的cur_char不为0，则将其加入到缓冲区kbd_buf中 */ 
				if (!ioq_full(&kbd_buf)) {
					// put_char(cur_char);		//临时的
					ioq_putchar(&kbd_buf, cur_char);
				}
				return;
			}
/* 记录本次是否按下了下面几类控制键之一，供下次键入时判断组合键 */
	        if (scancode == ctrl_l_make || scancode == ctrl_r_make) {
	        	ctrl_status = true;
	        } else if (scancode == shift_l_make || scancode == shift_r_make) {
	        	shift_status = true;
	        } else if (scancode == alt_l_make || scancode == alt_r_make) {
	        	alt_status = true;
	        } else if (scancode == caps_lock_make) {
/* 不管之前是否有按下caps_lock键，当再次按下时则状态取反，即已经开启时，再按下同样的键是关闭。关闭时按下表示开启*/
				caps_lock_status = !caps_lock_status;
			}
	} else {	//没有定义的通码
		put_str("unknown key\n", 0x07);
	}
}

/* 键盘初始化 */
void keyboard_init(void) 
{
	put_str("keyboard_init start\n", 0x07);
	ioqueue_init(&kbd_buf);
	register_handler(0x21, intr_keyboard_handler);
	put_str("keyboard_init done\n", 0x07);
}
