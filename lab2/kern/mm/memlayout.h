/*
 * ============================================================================
 * kern/mm/memlayout.h —— 物理内存布局与页描述符（Lab2 答辩必问）
 * ============================================================================
 * 【为什么需要这个文件】
 *   内核要管理物理页，就必须先知道三件事：
 *     1. 物理内存在哪里、多大（本实验从设备树 DTB 读）；
 *     2. 内核自己被放在哪里（0x80200000，OpenSBI 之后）；
 *     3. 物理地址和内核虚拟地址之间差多少（直接映射的偏移）。
 *
 * 【答辩要点一：KERNBASE 与 PHYSICAL_MEMORY_OFFSET 的关系】
 *   内核镜像的物理起始是 0x80200000，虚拟起始是 0xFFFFFFFFC0200000，
 *   差值固定为 0xFFFFFFFF40000000。这就是 va_pa_offset：
 *
 *     虚拟地址 = 物理地址 + va_pa_offset
 *
 *   所以可以直接用"物理地址 + 偏移"访问任意物理页，不需要每次走页表。
 *
 * 【答辩要点二：0x88000000 是什么】
 *   QEMU virt 默认 128 MiB RAM：0x80000000 ~ 0x88000000。
 *   低 2 MiB 被 OpenSBI 占用，内核从 0x80200000 开始。
 *   PHYSICAL_MEMORY_END 是管理上界；npage 是这个上界对应的"绝对页号"，
 *   不是页数。可用页数还要减去 nbase 和内核/描述符占用。
 *
 * 【答辩要点三：struct Page 不是物理页本身】
 *   它是每个物理页的"身份证"（描述符），本身放在内存里。
 *   32 32768 个页 × 40 字节 ≈ 1.25 MiB，这部分也必须从可用内存里扣除，
 *   否则写入描述符会覆盖用户数据。
 * ============================================================================
 */
#ifndef __KERN_MM_MEMLAYOUT_H__
#define __KERN_MM_MEMLAYOUT_H__

/* ===== 直接映射的地址常量 =====
 * KERNBASE = 内核镜像的虚拟起始地址
 *          = 物理 0x80200000 + 偏移 0xFFFFFFFF40000000
 * KMEMSIZE = 内核可管理的物理内存大小（128 MiB - 2 MiB）
 * KERNTOP  = 直接映射区的虚拟上界，对应物理 0x88000000
 */
/* All physical memory mapped at this address */
#define KERNBASE            0xFFFFFFFFC0200000 // = 0x80200000 + 0xFFFFFFFF40000000
//把原有内存映射到虚拟内存空间的最后一页
#define KMEMSIZE            0x7E00000          // the maximum amount of physical memory
// 0x7E00000 = 0x8000000 - 0x200000
// QEMU 缺省的RAM为 0x80000000到0x88000000, 128MiB, 0x80000000到0x80200000被OpenSBI占用
#define KERNTOP             (KERNBASE + KMEMSIZE) // 0x88000000对应的虚拟地址

#define PHYSICAL_MEMORY_END         0x88000000   // 物理内存上界（不含）
#define PHYSICAL_MEMORY_OFFSET      0xFFFFFFFF40000000 // 直接映射偏移
#define KERNEL_BEGIN_PADDR          0x80200000   // 内核物理起始
#define KERNEL_BEGIN_VADDR          0xFFFFFFFFC0200000 // 内核虚拟起始

/* 内核栈占 2 页（8 KiB）。启动栈也在 entry.S 里按这个大小定义。
 * 答辩提醒：本实验所有测试的大数组都必须放 .bss，不能压栈。 */
#define KSTACKPAGE          2                           // # of pages in kernel stack
#define KSTACKSIZE          (KSTACKPAGE * PGSIZE)       // sizeof kernel stack

#ifndef __ASSEMBLER__

#include <defs.h>
#include <list.h>

/* 页表项/页目录项都是 64 位，直接用 uintptr_t 表示。 */
typedef uintptr_t pte_t;
typedef uintptr_t pde_t;

/* *
 * struct Page —— 一个物理页的描述符（不是页本身！）
 *
 *   ref      引用计数：分配出去时自己决定怎么用；free 时要求为 0
 *   flags    标志位：PG_reserved（内核保留，不可分配）、
 *            PG_property（当前页是空闲块/阶块的头）
 *   property 空闲块长度（First-Fit/Best-Fit）或阶数（Buddy）
 *   page_link 挂到空闲链表上的节点
 *
 * 【答辩要点】为什么 property 在 Buddy 里表示阶数？
 *   三种分配器共用同一个 struct Page，字段含义由具体分配器约定。
 *   First-Fit/Best-Fit 用 property 存"块有多少页"；
 *   Buddy 用 property 存"块是 2^order 页"，并额外在已分配块首记录 order，
 *   这样 free_pages(base, n) 才能还原真实块大小。
 * */
/* *
 * struct Page - Page descriptor structures. Each Page describes one
 * physical page. In kern/mm/pmm.h, you can find lots of useful functions
 * that convert Page to other data types, such as physical address.
 * */
struct Page {
    int ref;                        // page frame's reference counter
    uint64_t flags;                 // array of flags that describe the status of the page frame
    unsigned int property;          // num of pages in a free block (First/Best-Fit) or block order (Buddy)
    list_entry_t page_link;         // free list link
};

/* ===== 标志位 =====
 * PG_reserved=1：内核/固件/描述符占用，分配器绝不能把它交出去。
 * PG_property=1：该页是空闲块的"头"。
 *   - 只有头页会挂进空闲链表；
 *   - 分配后头页的 PG_property 必须清零，否则会被误认为还空闲。
 */
/* Flags describing the status of a page frame */
#define PG_reserved                 0       // reserved for kernel, never handed out
#define PG_property                 1       // head page of a free block

#define SetPageReserved(page)       ((page)->flags |= (1UL << PG_reserved))
#define ClearPageReserved(page)     ((page)->flags &= ~(1UL << PG_reserved))
#define PageReserved(page)          (((page)->flags >> PG_reserved) & 1)
#define SetPageProperty(page)       ((page)->flags |= (1UL << PG_property))
#define ClearPageProperty(page)     ((page)->flags &= ~(1UL << PG_property))
#define PageProperty(page)          (((page)->flags >> PG_property) & 1)

/* 从链表节点反推所在的 struct Page。
 * to_struct 利用 container_of 的思想：用成员地址减去成员偏移。 */
// convert list entry to page
#define le2page(le, member)                 \
    to_struct((le), struct Page, member)

/* free_area_t：一条空闲链表 + 该链表上的总页数。
 * First-Fit/Best-Fit 只用一条；Buddy 每阶一条。 */
/* free_area_t - maintains a doubly linked list to record free (unused) pages */
typedef struct {
    list_entry_t free_list;         // the list header
    unsigned int nr_free;           // number of free pages in this free list
} free_area_t;

#endif /* !__ASSEMBLER__ */

#endif /* !__KERN_MM_MEMLAYOUT_H__ */
