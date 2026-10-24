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
