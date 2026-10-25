/*
 * ============================================================================
 * slub.h —— 简化 SLUB 对象分配器接口
 * ============================================================================
 * 提供 slub_init/kmalloc/kfree。slub_check 是实现者自检，不是成员三的独立测试。
 * ============================================================================
 */
#ifndef __KERN_MM_SLUB_H__
#define __KERN_MM_SLUB_H__
#include <defs.h>

// Single-hart teaching allocator. Call slub_init once after pmm_init.
void slub_init(void);
void *kmalloc(size_t size);
void kfree(void *ptr);
// Implementation smoke checks; member three should add independent tests.
void slub_check(void);
#endif
