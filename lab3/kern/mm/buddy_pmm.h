/*
 * ============================================================================
 * 【代码归属】成员三实现
 * 【判定依据】按 lab2.docx 的任务划分与代码内容判断，不依赖可能被清除的 lab2 / YOUR CODE 标记。
 * ============================================================================
 * 【答辩可以直接这样念】
 *
 *   “这里导出 buddy_pmm_manager，以及两个只读的测试辅助：
 *    buddy_order_of 用来验证 n 页应该落到哪一阶，
 *    buddy_free_blocks 返回某阶链表上的块数，供测试比较空闲区‘形状’。”

 * ============================================================================
 * 【本组改动】本文件由成员三实现。
 * ============================================================================
 */
/*
 * ============================================================================
 * buddy_pmm.h —— Buddy System 管理器接口
 * ============================================================================
 * 提供 buddy_pmm_manager；buddy_order_of/buddy_free_blocks 供独立测试做只读检查。
 * ============================================================================
 */
#ifndef __KERN_MM_BUDDY_PMM_H__
#define __KERN_MM_BUDDY_PMM_H__

#include <pmm.h>

extern const struct pmm_manager buddy_pmm_manager;

/* Order of the block that alloc_pages(n) would hand out for a request of n
 * pages. Exposed so tests can check the rounding contract. */
size_t buddy_order_of(size_t n);

/* Used by the test code to inspect the free lists without reaching into the
 * allocator's statics. Returns the number of blocks currently in free list
 * `order`. */
size_t buddy_free_blocks(size_t order);

#endif /* ! __KERN_MM_BUDDY_PMM_H__ */
