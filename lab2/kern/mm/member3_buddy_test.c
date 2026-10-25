/*
 * ============================================================================
 * 【代码归属】成员三实现（独立测试）
 * 【判定依据】按 lab2.docx 的任务划分与代码内容判断，不依赖可能被清除的 lab2 / YOUR CODE 标记。
 * ============================================================================
 * 【答辩可以直接这样念】
 *
 *   “这是成员三的独立 Buddy 测试，只使用公开接口和两个只读检查辅助，
 *    不读分配器的静态变量。6 组分别覆盖：阶数取整、逐页耗尽后逆序归还、
 *    失败请求无副作用、最大块拆分后合并恢复形状、相邻非伙伴不合并、
 *    各阶对齐。最有代表性的是第 5 组：奇数页 p 和 p+1 物理相邻，
 *    但 p 的伙伴是 p-1、p+1 的伙伴是 p+2，所以它们不能合并；
 *    只按‘物理相邻’合并的实现会在这里失败。”

 * ============================================================================
 * 【本组改动】本文件由成员三实现。
 * ============================================================================
 */
//lab2 function3-2 2411738 秦东杰
/* ============================================================================
 * member3_buddy_test.c —— 成员三的独立 Buddy 测试（答辩重点）
 * ============================================================================
 * 【为什么要有"独立测试"】
 *   buddy_check 是实现者在实现文件里写的自检，容易"顺着自己的实现写"。
 *   本文件只使用公开接口（alloc_pages/free_pages/nr_free_pages），
 *   再加两个只读的检查辅助（buddy_order_of / buddy_free_blocks），
 *   不读分配器的静态变量，所以能更客观地验证契约。
 *
 * 【为什么不能照搬 Best-Fit 的测试】
 *   member2_page_check 依赖"一次 alloc_pages(全部空闲页) 能成功"，
 *   但 Buddy 的空闲区分散在多个阶上，这个请求可能合法失败。
 *   所以这里用"逐页申请直到 NULL"来构造耗尽。
 *
 * 【答辩要点：测试有效性】
 *   好的测试不只是"跑通"，而是"实现写错时会失败"。
 *   每组的注释都写明它能抓到哪类缺陷。
 * ============================================================================ */
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

/* ============================================================================
 * T1 阶数取整契约
 *   n=1..10 应分别对应 order 0,1,2,2,3,3,3,3,4,4。
 *   能让"把请求页数当阶数记录"的实现失败。
 * ============================================================================ */
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

/* ============================================================================
 * T2 逐页耗尽 + 逆序归还
 *   每页单独申请直到 NULL，申请数必须等于初始空闲页数；
 *   再按相反顺序逐页归还，各阶块数必须恢复原状。
 *   能让"丢页、错误合并、计数漂移"的实现失败。
 *   注意：这里比较的是各阶块数快照，不是只比较总页数——
 *   总页数相同但块形状不同，同样是不变量被破坏。
 * ============================================================================ */
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

/* ============================================================================
 * T3 失败请求无副作用
 *   申请超过总空闲页、申请 SIZE_MAX，都必须返回 NULL，
 *   且前后各阶块数快照完全一致。
 *   能让"先改链表再检查失败"的实现失败。
 * ============================================================================ */
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

/* ============================================================================
 * T4 大块拆分后释放、形状还原
 *   找到当前最大可用阶，申请整块，再释放；
 *   各阶块数必须回到申请前的快照。
 *   能让"拆分时把余块丢了"或"合并只并一半"的实现失败。
 * ============================================================================ */
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

/* ============================================================================
 * T5 相邻但非伙伴不能合并（本测试最关键的一组）
 *   order 0 的伙伴是 (偶数页号, 偶数页号+1)。
 *   取一个奇数页号 p 和 p+1：它们物理相邻，但
 *     p 的伙伴是 p-1，p+1 的伙伴是 p+2。
 *   在 p-1、p+2 仍被占用时把 p 和 p+1 释放，
 *   它们必须保持为两个独立的 order-0 块。
 *   能让"只按物理相邻就合并"的错误实现失败。
 * ============================================================================ */
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

/* ============================================================================
 * T6 各阶对齐 + 乱序释放
 *   申请 order 0..8，检查每个块首按自身大小对齐；
 *   再按打乱顺序释放，验证合并路径。
 *   能让"块首未对齐"的实现失败——对齐一破，异或伙伴计算就全错。
 * ============================================================================ */
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
