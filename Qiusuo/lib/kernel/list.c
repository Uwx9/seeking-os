#include "list.h"
#include "interrupt.h"
#include "debug.h"
#include "print.h"

/* 初始化双向链表list */ 
void list_init(struct list* list)
{
	list->head.prev = NULL;
	list->head.next = &list->tail;
	list->tail.prev = &list->head;
	list->tail.next = NULL;
}

/* 把链表元素elem插入在元素before之前 */ 
void list_insert_before(struct list_elem* before, struct list_elem* elem)
{
	enum intr_status old_status = intr_disable();
	before->prev->next = elem;
	elem->prev = before->prev;
	elem->next = before;
	before->prev = elem;
	intr_set_status(old_status);

}

/* 添加元素到列表队首，类似栈push操作 */
void list_push(struct list* plist, struct list_elem* elem)
{
	list_insert_before(plist->head.next, elem);
}

/* 追加元素到链表队尾，类似队列的先进先出操作 */
void list_append(struct list* plist, struct list_elem* elem)
{
	list_insert_before(&plist->tail, elem);
}

/* 使元素pelem脱离链表 */
void list_remove(struct list_elem* pelem)
{
	enum intr_status old_status = intr_disable();
	pelem->prev->next = pelem->next;
	pelem->next->prev = pelem->prev;
	intr_set_status(old_status);
}

void ready_tasks_num(struct list* plist)
{
	int cnt = 0; 
	struct list_elem* elem = plist->head.next;
	while (elem != &plist->tail) {
		cnt++;
		elem = elem->next;
	}
	put_str("task num is:");put_int(cnt);
}

/* 将链表第一个元素弹出并返回，类似栈的pop操作 */
struct list_elem* list_pop(struct list* plist)
{
	if (list_empty(plist)) {  // 先判断是否为空
		return NULL;
	}
	struct list_elem* elem = plist->head.next;
	list_remove(elem);
	return elem;
}

/* 从链表中查找元素obj_elem，成功时返回true，失败时返回false */
bool elem_find(struct list* plist, struct list_elem* obj_elem)
{
	struct list_elem* mov = plist->head.next;
	while (mov != &plist->tail) {
		if (mov == obj_elem) {
			return true;
		}
		mov = mov->next;
	}
	return false;
}

/* 把列表plist中的每个元素elem和arg传给回调函数func， 
 * arg 给func用来判断elem是否符合条件． 
 * 本函数的功能是遍历列表内所有元素，逐个判断是否有符合条件的元素。 
 * 找到符合条件的元素返回元素指针，否则返回NULL 
 */ 
struct list_elem* list_traversal(struct list* plist, function func, int arg)
{
	struct list_elem* elem = plist->head.next;
	if (list_empty(plist)) {
		return NULL;
	}
	while (elem != &plist->tail) {
		/* func 返回ture，则认为该元素在回调函数中符合条件，命中，故停止继续遍历 */
		if (func(elem, arg)) {
			return elem;
		}
		elem = elem->next;
	}
	return NULL;
}

uint32_t list_len(struct list* plist)
{
	struct list_elem* elem = plist->head.next;
	uint32_t length = 0;
	while (elem != &plist->tail) {
		length++;
		elem = elem->next;
	}
	return length;
}

bool list_empty(struct list* plist)
{
	return plist->head.next == &plist->tail ? true : false;
}