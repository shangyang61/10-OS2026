/*
 * ============================================================================
 * 【代码归属】课程实验框架提供（不是本组实现）
 * 【判定依据】按 lab2.docx 的任务划分与代码内容判断，不依赖可能被清除的 lab2 / YOUR CODE 标记。
 * ============================================================================
 * 【答辩可以直接这样念】
 *
 *   “这是内核通用双向链表。它用哨兵节点，空链时 prev 和 next 都指回自己，
 *    遍历结束条件是回到 head，而不是 NULL，这样插入删除不用特判头尾。
 *    链表是侵入式的：节点长在 struct Page 里，不是单独分配，
 *    所以从节点找回对象要用 le2page → to_struct，也就是成员地址减偏移。”

 * ============================================================================
 * 【本组改动】未改动。
 * ============================================================================
 */
/*
 * ============================================================================
 * libs/list.h —— 内核通用双向链表（Lab2 答辩必问）
 * ============================================================================
 * 【为什么内核要自己写链表】
 *   不能依赖 glibc。页分配器的空闲链、SLUB 的 partial 链都靠它。
 *
 * 【答辩要点一：哨兵节点】
 *   链表头本身不是一个数据节点，而是一个"哨兵"：
 *   初始化时 prev = next = 自己，表示空链。
 *   遍历的结束条件就是 le == head，而不是 le == NULL。
 *   好处：插入/删除不用特判头尾，代码统一。
 *
 * 【答辩要点二：侵入式链表】
 *   struct Page 里嵌入一个 list_entry_t page_link，
 *   链表节点不是单独 malloc 出来的，而是"长"在对象里。
 *   这样一次分配就同时拿到对象和链表节点，没有额外内存开销。
 *   反过来从节点找回对象要用 le2page → to_struct（容器宏）。
 *
 * 【答辩要点三：list_del 与 list_del_init 的区别】
 *   list_del 只把前后节点接起来，被删节点的 prev/next 仍是脏值；
 *   list_del_init 额外把自己指回自己。
 *   如果删完还要判断"它在不在链上"，必须用后者。
 * ============================================================================
 */
#ifndef __LIBS_LIST_H__
#define __LIBS_LIST_H__

#ifndef __ASSEMBLER__

#include <defs.h>

/* *
 * Simple doubly linked list implementation.
 *
 * Some of the internal functions ("__xxx") are useful when manipulating
 * whole lists rather than single entries, as sometimes we already know
 * the next/prev entries and we can generate better code by using them
 * directly rather than using the generic single-entry routines.
 * */

/* 双向链表节点：只有前驱和后继，没有数据字段（数据在宿主结构里）。 */
struct list_entry {
    struct list_entry *prev, *next;
};

typedef struct list_entry list_entry_t;

static inline void list_init(list_entry_t *elm) __attribute__((always_inline));
static inline void list_add(list_entry_t *listelm, list_entry_t *elm) __attribute__((always_inline));
static inline void list_add_before(list_entry_t *listelm, list_entry_t *elm) __attribute__((always_inline));
static inline void list_add_after(list_entry_t *listelm, list_entry_t *elm) __attribute__((always_inline));
static inline void list_del(list_entry_t *listelm) __attribute__((always_inline));
static inline void list_del_init(list_entry_t *listelm) __attribute__((always_inline));
static inline bool list_empty(list_entry_t *list) __attribute__((always_inline));
static inline list_entry_t *list_next(list_entry_t *listelm) __attribute__((always_inline));
static inline list_entry_t *list_prev(list_entry_t *listelm) __attribute__((always_inline));

static inline void __list_add(list_entry_t *elm, list_entry_t *prev, list_entry_t *next) __attribute__((always_inline));
static inline void __list_del(list_entry_t *prev, list_entry_t *next) __attribute__((always_inline));

/* *
 * list_init - initialize a new entry
 * @elm:        new entry to be initialized
 * */
/* 把节点初始化为自环：既可作为空链表头，也可作为"不在链上"的初始状态。 */
static inline void
list_init(list_entry_t *elm) {
    elm->prev = elm->next = elm;
}

/* *
 * list_add - add a new entry
 * @listelm:    list head to add after
 * @elm:        new entry to be added
 *
 * Insert the new element @elm *after* the element @listelm which
 * is already in the list.
 * */
/* 把 elm 插到 listelm 之后（头插时 listelm 就是哨兵）。 */
static inline void
list_add(list_entry_t *listelm, list_entry_t *elm) {
    list_add_after(listelm, elm);
}

/* *
 * list_add_before - add a new entry
 * @listelm:    list head to add before
 * @elm:        new entry to be added
 *
 * Insert the new element @elm *before* the element @listelm which
 * is already in the list.
 * */
/* 把 elm 插到 listelm 之前。分配器按地址有序插入时常用。 */
static inline void
list_add_before(list_entry_t *listelm, list_entry_t *elm) {
    __list_add(elm, listelm->prev, listelm);
}

/* *
 * list_add_after - add a new entry
 * @listelm:    list head to add after
 * @elm:        new entry to be added
 *
 * Insert the new element @elm *after* the element @listelm which
 * is already in the list.
 * */
static inline void
list_add_after(list_entry_t *listelm, list_entry_t *elm) {
    __list_add(elm, listelm, listelm->next);
}

/* *
 * list_del - deletes entry from list
 * @listelm:    the element to delete from the list
 *
 * Note: list_empty() on @listelm does not return true after this, the entry is
 * in an undefined state.
 * */
/* 从链上摘除，但不重置自身指针；调用后不要再用 list_empty 判断。 */
static inline void
list_del(list_entry_t *listelm) {
    __list_del(listelm->prev, listelm->next);
}

/* *
 * list_del_init - deletes entry from list and reinitialize it.
 * @listelm:    the element to delete from the list.
 *
 * Note: list_empty() on @listelm returns true after this.
 * */
/* 摘除并把自身恢复成自环，之后可以安全地再次插入或判断。 */
static inline void
list_del_init(list_entry_t *listelm) {
    list_del(listelm);
    list_init(listelm);
}

/* *
 * list_empty - tests whether a list is empty
 * @list:       the list to test.
 * */
/* 空链判据：哨兵的 next 指回自己。 */
static inline bool
list_empty(list_entry_t *list) {
    return list->next == list;
}

/* *
 * list_next - get the next entry
 * @listelm:    the list head
 **/
static inline list_entry_t *
list_next(list_entry_t *listelm) {
    return listelm->next;
}

/* *
 * list_prev - get the previous entry
 * @listelm:    the list head
 **/
static inline list_entry_t *
list_prev(list_entry_t *listelm) {
    return listelm->prev;
}

/* *
 * Insert a new entry between two known consecutive entries.
 *
 * This is only for internal list manipulation where we know
 * the prev/next entries already!
 * */
/* 已知前后邻居时的插入：四根指针两两接好。 */
static inline void
__list_add(list_entry_t *elm, list_entry_t *prev, list_entry_t *next) {
    prev->next = next->prev = elm;
    elm->next = next;
    elm->prev = prev;
}

/* *
 * Delete a list entry by making the prev/next entries point to each other.
 *
 * This is only for internal list manipulation where we know
 * the prev/next entries already!
 * */
/* 已知前后邻居时的删除：让它们互相指向。 */
static inline void
__list_del(list_entry_t *prev, list_entry_t *next) {
    prev->next = next;
    next->prev = prev;
}

#endif /* !__ASSEMBLER__ */

#endif /* !__LIBS_LIST_H__ */

