//lab2 function3-2 2411738 秦东杰
/* Independent Buddy System tests, written by member three.
 *
 * These use only the public page interface plus the two inspection helpers in
 * buddy_pmm.h. They are written from the interface contract rather than from
 * the implementation, and each case says what a broken allocator would do
 * differently.
 *
 * Design note: the boot stack is only 2 pages (8 KiB), so anything that needs
 * many pointers uses a static array and nothing here recurses. Nothing asks
 * for "all free pages in one call": under Buddy the free space is split over
 * several orders, so such a request may legitimately fail. */

#include <buddy_pmm.h>
#include <member3_check.h>
#include <pmm.h>
#include <stdio.h>

/* Buddy keeps orders 0..15 (16 lists of at most 2^15 pages). */
#define TEST_ORDERS 16

static struct Page *held[9];
static struct Page *pages_seen[64];

static size_t
t_pow2(size_t order) {
    return (size_t)1 << order;
}

static void
snapshot(size_t *out) {
    for (size_t order = 0; order < TEST_ORDERS; ++order) {
        out[order] = buddy_free_blocks(order);
    }
}

static bool
same_snapshot(const size_t *a, const size_t *b) {
    for (size_t order = 0; order < TEST_ORDERS; ++order) {
        if (a[order] != b[order]) {
            return 0;
        }
    }
    return 1;
}

static size_t
total_pages(const size_t *s) {
    size_t pages = 0;
    for (size_t order = 0; order < TEST_ORDERS; ++order) {
        pages += s[order] * t_pow2(order);
    }
    return pages;
}

/* T1: rounding contract. A request that is not a power of two must still
 * record its real block order. An allocator that stored the request instead
 * would report 3 for alloc_pages(3) and could not size the free correctly. */
static void
t1_rounding(void) {
    /* n:      1  2  3  4  5  6  7  8  9 10
     * 2^k >= n: 1  2  4  4  8  8  8  8 16 16  => k: 0 1 2 2 3 3 3 3 4 4 */
    static const size_t expect_order[] = {0, 1, 2, 2, 3, 3, 3, 3, 4, 4};
    for (size_t n = 1; n <= 10; ++n) {
        size_t order = buddy_order_of(n);
        if (order != expect_order[n - 1]) {
            panic("member3 T1: order_of(%d)=%d, expected %d", (int)n,
                  (int)order, (int)expect_order[n - 1]);
        }
    }
    for (size_t n = 1; n <= 10; ++n) {
        struct Page *p = alloc_pages(n);
        assert(p != NULL);
        if (p->property != buddy_order_of(n)) {
            panic("member3 T1: alloc_pages(%d) recorded order %d, expected %d",
                  (int)n, (int)p->property, (int)buddy_order_of(n));
        }
        if ((page2ppn(p) & (t_pow2(p->property) - 1)) != 0) {
            panic("member3 T1: block for %d pages is not aligned", (int)n);
        }
        free_pages(p, n);
    }
    cprintf("member3 T1 rounding contract: PASS\n");
}

/* T2: exhaust the machine one page at a time, then give everything back in
 * reverse order and require the exact original free-list shape.
 * A leak or a bogus merge would show up as a different shape, not just a
 * different total. */
static void
t2_exhaust_and_recover(void) {
    size_t before = nr_free_pages();
    size_t snap_before[TEST_ORDERS], snap_after[TEST_ORDERS];
    snapshot(snap_before);
    if (total_pages(snap_before) != before) {
        panic("member3 T2: free lists hold %d pages, nr_free says %d",
              (int)total_pages(snap_before), (int)before);
    }

    /* Ask for a page one at a time so fragmented free space is still usable. */
    size_t taken = 0;
    struct Page *p;
    while ((p = alloc_page()) != NULL) {
        ++taken;
    }
    if (taken != before) {
        panic("member3 T2: took %d single pages, expected %d", (int)taken,
              (int)before);
    }
    if (nr_free_pages() != 0) {
        panic("member3 T2: %d pages still free after exhaustion",
              (int)nr_free_pages());
    }
    /* With nothing free, every further request must fail cleanly. */
    assert(alloc_page() == NULL);
    assert(alloc_pages(2) == NULL);
    assert(alloc_pages(before) == NULL);

    /* We cannot keep pointers to every page on the stack, so rebuild the
     * bookkeeping from the descriptor array: each taken page is exactly one
     * page that is no longer reserved and no longer a free head. */
    size_t returned = 0;
    for (size_t i = npage - nbase; i > 0; --i) {
        struct Page *q = &pages[i - 1];
        if (!PageReserved(q) && !PageProperty(q)) {
            free_pages(q, 1);
            ++returned;
        }
    }
    if (returned != before) {
        panic("member3 T2: returned %d pages, expected %d", (int)returned,
              (int)before);
    }
    snapshot(snap_after);
    if (!same_snapshot(snap_before, snap_after)) {
        panic("member3 T2: free list shape was not restored");
    }
    cprintf("member3 T2 exhaust and recover: PASS (%d pages)\n", (int)taken);
}

/* T3: a failing request must not perturb the free lists. */
static void
t3_oom_is_clean(void) {
    size_t before = nr_free_pages();
    size_t snap_before[TEST_ORDERS], snap_after[TEST_ORDERS];
    snapshot(snap_before);

    /* More than the whole machine: must fail. */
    assert(alloc_pages(before + 1) == NULL);
    /* Absurd size must not overflow into a small order. */
    assert(alloc_pages((size_t)-1) == NULL);

    snapshot(snap_after);
    if (!same_snapshot(snap_before, snap_after)) {
        panic("member3 T3: failed allocation changed the free lists");
    }
    if (nr_free_pages() != before) {
        panic("member3 T3: nr_free drifted to %d", (int)nr_free_pages());
    }
    cprintf("member3 T3 failed request is side-effect free: PASS\n");
}

/* T4: splitting a large block and freeing it again must restore the shape. */
static void
t4_split_merge(void) {
    size_t before[TEST_ORDERS], after[TEST_ORDERS];
    snapshot(before);
    size_t order = TEST_ORDERS - 1;
    while (order > 0 && buddy_free_blocks(order) == 0) {
        --order;
    }
    if (buddy_free_blocks(order) == 0) {
        panic("member3 T4: no free block found at any order");
    }
    size_t want = t_pow2(order);
    struct Page *big = alloc_pages(want);
    assert(big != NULL);
    if (big->property != order) {
        panic("member3 T4: largest block came back as order %d, expected %d",
              (int)big->property, (int)order);
    }
    if (nr_free_pages() != total_pages(before) - want) {
        panic("member3 T4: page count wrong after large allocation");
    }
    free_pages(big, want);
    snapshot(after);
    if (!same_snapshot(before, after)) {
        panic("member3 T4: split then merge did not restore the free lists");
    }
    cprintf("member3 T4 split then merge restores shape: PASS (order %d)\n",
            (int)order);
}

/* T5: only true buddies merge.
 *
 * Order-0 buddies are the pairs (even pfn, even pfn + 1). So an odd pfn and
 * its successor are physically adjacent but are NOT buddies: page p is paired
 * with p-1, and page p+1 with p+2. If those two are freed while both
 * neighbours are still allocated, they must stay two separate order-0 blocks.
 * An allocator that merged on physical adjacency alone would fuse them and
 * break the buddy invariant. */
static void
t5_non_buddy(void) {
    /* Around the free region, collect a run of single pages. */
    size_t count = 0;
    while (count < 64) {
        struct Page *p = alloc_page();
        if (p == NULL) {
            break;
        }
        pages_seen[count++] = p;
    }
    /* Find an odd pfn with its successor also in hand and both neighbours of
     * the pair still allocated. */
    struct Page *low = NULL, *high = NULL;
    for (size_t i = 0; i + 1 < count; ++i) {
        size_t pfn = page2ppn(pages_seen[i]);
        if ((pfn & 1) && page2ppn(pages_seen[i + 1]) == pfn + 1) {
            /* p-1 must still be allocated: it is the buddy of p. */
            bool neighbour_held = 0;
            for (size_t j = 0; j < count; ++j) {
                if (page2ppn(pages_seen[j]) == pfn - 1) {
                    neighbour_held = 1;
                }
            }
            if (neighbour_held) {
                low = pages_seen[i];
                high = pages_seen[i + 1];
                break;
            }
        }
    }
    if (low == NULL) {
        /* Not fatal: the layout did not offer this exact run. */
        cprintf("member3 T5 non-buddy: SKIP (no suitable run)\n");
        for (size_t j = 0; j < count; ++j) {
            free_pages(pages_seen[j], 1);
        }
        return;
    }
    size_t o0 = buddy_free_blocks(0), o1 = buddy_free_blocks(1);
    free_pages(low, 1);
    free_pages(high, 1);
    if (buddy_free_blocks(0) != o0 + 2 || buddy_free_blocks(1) != o1) {
        panic("member3 T5: physically adjacent non-buddies were merged");
    }
    /* Give the rest back; low and high were already released above. */
    for (size_t j = 0; j < count; ++j) {
        struct Page *q = pages_seen[j];
        if (q == low || q == high) {
            continue;
        }
        free_pages(q, 1);
    }
    assert(nr_free_pages() >= o0 + o1);
    cprintf("member3 T5 adjacent non-buddies stay split: PASS\n");
}

/* T6: every returned block must be aligned on its own size; that is the
 * property the XOR buddy computation depends on. */
static void
t6_alignment(void) {
    for (size_t order = 0; order <= 8; ++order) {
        struct Page *p = alloc_pages(t_pow2(order));
        assert(p != NULL);
        if ((page2ppn(p) & (t_pow2(order) - 1)) != 0) {
            panic("member3 T6: order %d block at pfn %d is misaligned",
                  (int)order, (int)page2ppn(p));
        }
        held[order] = p;
    }
    /* Release in a scrambled order so merging is exercised, not just LIFO. */
    free_pages(held[1], t_pow2(1));
    free_pages(held[7], t_pow2(7));
    free_pages(held[0], t_pow2(0));
    free_pages(held[5], t_pow2(5));
    free_pages(held[2], t_pow2(2));
    free_pages(held[8], t_pow2(8));
    free_pages(held[3], t_pow2(3));
    free_pages(held[6], t_pow2(6));
    free_pages(held[4], t_pow2(4));
    cprintf("member3 T6 alignment for orders 0..8: PASS\n");
}

void
member3_buddy_test(void) {
    cprintf("member3 buddy tests: start\n");
    size_t before = nr_free_pages();
    t1_rounding();
    t2_exhaust_and_recover();
    t3_oom_is_clean();
    t4_split_merge();
    t5_non_buddy();
    t6_alignment();
    if (nr_free_pages() != before) {
        panic("member3 buddy tests: leaked pages (%d != %d)",
              (int)nr_free_pages(), (int)before);
    }
    cprintf("member3_buddy_test() succeeded!\n");
}
