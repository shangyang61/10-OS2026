#include <buddy_pmm.h>
#include <defs.h>
#include <list.h>
#include <pmm.h>
#include <stdio.h>

//lab2 function3-1 2411738 秦东杰
/* *
 * Buddy System with explicit order bookkeeping.
 *
 * Blocks are power-of-two sized and aligned on their own size, which is what
 * makes `pfn ^ (1 << order)` identify the buddy. Free blocks are kept on one
 * list per order; only block heads carry PG_property.
 *
 * Contract that matters for the object layer above us:
 *
 *   alloc_pages(n) rounds n up to 2^k and returns an aligned block of 2^k
 *   pages, recording k in the head page's `property`. free_pages(base, n)
 *   checks that the recorded block is exactly the block that owns the
 *   request and returns the *whole* 2^k block, not just n pages.
 *
 * Without that record a caller that asked for 3 pages and received a 4-page
 * block could only release 3 pages, silently leaking the fourth, or worse,
 * split a block that is still in use. Recording the order on the head page
 * lets us round-trip allocations whose size is not a power of two.
 * */

#define MAX_ORDER 16 // 2^15 pages = 128 MiB, the largest reachable block
#define NR_ORDERS MAX_ORDER

static list_entry_t free_areas[NR_ORDERS];
static size_t nr_free;
static bool buddy_ready;

static void buddy_check(void);

static size_t
pow2(size_t order) {
    assert(order < NR_ORDERS);
    return (size_t)1 << order;
}

/* 2^k, or 0 when k is out of range so the shift never happens. */
static size_t
pow2_or_zero(size_t order) {
    if (order >= NR_ORDERS) {
        return 0;
    }
    return (size_t)1 << order;
}

/* Smallest k with 2^k >= n, or NR_ORDERS when no block on this machine can
 * hold n pages. Returning a saturating value (instead of looping) matters:
 * callers pass lengths like (size_t)-1 and must get a clean NULL, not a hang. */
size_t
buddy_order_of(size_t n) {
    size_t order = 0;
    while (order < NR_ORDERS && pow2_or_zero(order) < n) {
        ++order;
    }
    return order;
}

static struct Page *
buddy_of(struct Page *page, size_t order) {
    size_t index = (size_t)(page - pages);
    size_t buddy_index = index ^ pow2(order);
    if (buddy_index >= npage - nbase) {
        return NULL;
    }
    return &pages[buddy_index];
}

/* Both neighbours free and at the same order is exactly the merge condition. */
static bool
buddy_mergeable(struct Page *page, size_t order) {
    struct Page *buddy = buddy_of(page, order);
    return buddy != NULL && PageProperty(buddy) && buddy->property == order;
}

/* Insert a free block, folding buddies together while they match.
 *
 * nr_free grows by the size of the block being returned, counted once and
 * *before* merging: the buddy it may merge with is already on a free list and
 * already included in nr_free. Adding the merged order instead would count
 * the partner's pages a second time. */
static void
buddy_insert(struct Page *block, size_t order) {
    nr_free += pow2(order);
    while (order + 1 < NR_ORDERS && buddy_mergeable(block, order)) {
        struct Page *buddy = buddy_of(block, order);
        list_del(&buddy->page_link);
        ClearPageProperty(buddy);
        buddy->property = 0;
        ClearPageProperty(block);
        block->property = 0;
        if (buddy < block) {
            block = buddy;
        }
        ++order;
    }
    block->property = order;
    SetPageProperty(block);
    list_add(&free_areas[order], &block->page_link);
}

/* Drop the flag and length record from every page we are about to hand out. */
static void
buddy_mark_allocated(struct Page *block, size_t order) {
    for (size_t i = 0; i < pow2(order); ++i) {
        struct Page *p = block + i;
        ClearPageProperty(p);
        ClearPageReserved(p);
        set_page_ref(p, 0);
        p->property = 0;
    }
    block->property = order; // Real order, so free_pages can recover it.
}

static void
buddy_init(void) {
    for (size_t i = 0; i < NR_ORDERS; ++i) {
        list_init(&free_areas[i]);
    }
    nr_free = 0;
    buddy_ready = 1;
}

size_t
buddy_free_blocks(size_t order) {
    if (order >= NR_ORDERS) {
        return 0;
    }
    size_t count = 0;
    list_entry_t *le = &free_areas[order];
    while ((le = list_next(le)) != &free_areas[order]) {
        ++count;
    }
    return count;
}

/* Split [base, base+n) into the largest natural blocks we can form. */
static void
buddy_init_memmap(struct Page *base, size_t n) {
    assert(n > 0);
    assert(buddy_ready);
    struct Page *cur = base;
    size_t remaining = n;
    while (remaining > 0) {
        size_t pfn = page2ppn(cur);
        size_t order = 0;
        /* Grow while the block still fits and stays aligned on its own size. */
        while (order + 1 < NR_ORDERS && pow2(order + 1) <= remaining &&
               (pfn & (pow2(order + 1) - 1)) == 0) {
            ++order;
        }
        for (size_t i = 0; i < pow2(order); ++i) {
            struct Page *p = cur + i;
            assert(PageReserved(p));
            p->flags = 0;
            p->property = 0;
            set_page_ref(p, 0);
            list_init(&p->page_link);
        }
        buddy_insert(cur, order);
        cur += pow2(order);
        remaining -= pow2(order);
    }
}

static struct Page *
buddy_alloc_pages(size_t n) {
    assert(n > 0 && buddy_ready);
    size_t need = buddy_order_of(n);
    if (need >= NR_ORDERS || n > nr_free) {
        return NULL; // Enough total pages can still be too fragmented.
    }
    size_t order = need;
    while (order < NR_ORDERS && list_empty(&free_areas[order])) {
        ++order;
    }
    if (order == NR_ORDERS) {
        return NULL;
    }
    struct Page *block = le2page(list_next(&free_areas[order]), page_link);
    list_del(&block->page_link);
    ClearPageProperty(block);
    block->property = 0;
    nr_free -= pow2(order);

    /* Hand the upper halves back, keeping the low block for the caller. */
    while (order > need) {
        --order;
        struct Page *upper = block + pow2(order);
        upper->property = order;
        SetPageProperty(upper);
        list_add(&free_areas[order], &upper->page_link);
        nr_free += pow2(order);
    }
    buddy_mark_allocated(block, need);
    return block;
}

static void
buddy_free_pages(struct Page *base, size_t n) {
    assert(n > 0 && buddy_ready);
    assert(base != NULL);
    size_t order = base->property; // Order recorded by alloc_pages.
    assert(order < NR_ORDERS);
    assert(!PageReserved(base));
    assert(!PageProperty(base)); // Already-free head means a double free.
    /* The request must be the one that produced this block, otherwise the
     * caller and the allocator disagree about the block size. */
    assert(pow2(order) >= n);
    assert(order == 0 || n > pow2(order - 1));

    for (size_t i = 0; i < pow2(order); ++i) {
        struct Page *p = base + i;
        assert(!PageReserved(p));
        assert(!PageProperty(p));
        p->flags = 0;
        p->property = 0;
        set_page_ref(p, 0);
    }
    buddy_insert(base, order);
}

static size_t
buddy_nr_free_pages(void) {
    return nr_free;
}

/* Recompute the page count from the lists; anything else is a broken invariant. */
static size_t
buddy_count_free(void) {
    size_t total = 0;
    for (size_t order = 0; order < NR_ORDERS; ++order) {
        list_entry_t *le = &free_areas[order];
        while ((le = list_next(le)) != &free_areas[order]) {
            struct Page *p = le2page(le, page_link);
            assert(PageProperty(p));
            assert(p->property == order);
            /* Head must be aligned on the block size or the buddy math fails. */
            assert((page2ppn(p) & (pow2(order) - 1)) == 0);
            total += pow2(order);
        }
    }
    return total;
}

static void
buddy_check_orders(const size_t *expect, const char *stage) {
    for (size_t order = 0; order < NR_ORDERS; ++order) {
        size_t got = buddy_free_blocks(order);
        if (got != expect[order]) {
            panic("buddy: order %d block count %d != %d after %s\n",
                  (int)order, (int)got, (int)expect[order], stage);
        }
    }
}

static void
buddy_check(void) {
    assert(buddy_ready);
    assert(buddy_count_free() == nr_free);
    assert(nr_free <= npage - nbase);

    size_t before[NR_ORDERS];
    for (size_t order = 0; order < NR_ORDERS; ++order) {
        before[order] = buddy_free_blocks(order);
    }

    /* Allocation must be aligned on its own size and record its real order. */
    struct Page *blocks[9];
    for (size_t order = 0; order <= 8; ++order) {
        blocks[order] = buddy_alloc_pages(pow2(order));
        assert(blocks[order] != NULL);
        assert((page2ppn(blocks[order]) & (pow2(order) - 1)) == 0);
        assert(blocks[order]->property == order);
        assert(!PageProperty(blocks[order]));
    }

    /* Out-of-order release must merge back to the exact original shape. */
    free_pages(blocks[5], pow2(5));
    free_pages(blocks[2], pow2(2));
    free_pages(blocks[8], pow2(8));
    free_pages(blocks[0], pow2(0));
    free_pages(blocks[6], pow2(6));
    free_pages(blocks[1], pow2(1));
    free_pages(blocks[7], pow2(7));
    free_pages(blocks[3], pow2(3));
    free_pages(blocks[4], pow2(4));
    buddy_check_orders(before, "scrambled free");
    assert(buddy_count_free() == nr_free);

    /* A non power-of-two request still round-trips without leaking a page. */
    size_t saved = nr_free;
    struct Page *odd = alloc_pages(3);
    assert(odd != NULL);
    assert(odd->property == 2); // 3 pages need a 4-page block.
    free_pages(odd, 3);
    assert(nr_free == saved);
    buddy_check_orders(before, "odd size");

    /* Failure must not disturb the free lists. */
    struct Page *none = alloc_pages(nr_free + 1);
    assert(none == NULL);
    buddy_check_orders(before, "oom");
    assert(buddy_count_free() == nr_free);

    cprintf("buddy_check() succeeded! (alignment, order, split, merge, oom)\n");
}

const struct pmm_manager buddy_pmm_manager = {
    .name = "buddy_pmm_manager",
    .init = buddy_init,
    .init_memmap = buddy_init_memmap,
    .alloc_pages = buddy_alloc_pages,
    .free_pages = buddy_free_pages,
    .nr_free_pages = buddy_nr_free_pages,
    .check = buddy_check,
};
