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
