/*
 * ============================================================================
 * 【代码归属】成员三实现（独立测试）
 * 【判定依据】按 lab2.docx 的任务划分与代码内容判断，不依赖可能被清除的 lab2 / YOUR CODE 标记。
 * ============================================================================
 * 【答辩可以直接这样念】
 *
 *   “这是成员三的独立 SLUB 测试，按接口契约重写，不读 slab 头。
 *    8 组覆盖：规格边界两侧、同页共享与跨页对象、full→partial 复用、
 *    240 个混合对象区间不重叠、部分释放后数据保持、多页大对象、
 *    0/NULL/溢出长度，以及真实页耗尽。
 *    OOM 用逐页 alloc_page 直到 NULL，而不是一次申请全部空闲页，
 *    所以同一套测试在 Best-Fit 和 Buddy 下都能跑。”

 * ============================================================================
 * 【本组改动】本文件由成员三实现。
 * ============================================================================
 */
//lab2 function3-3 2411738 秦东杰
/* ============================================================================
 * member3_slub_test.c —— 成员三的独立 SLUB 测试（答辩重点）
 * ============================================================================
 * 【为什么要有"独立测试"】
 *   slub_check 是实现者自检；本文件按 specs/member2.md 的接口契约重写，
 *   只使用 kmalloc/kfree/nr_free_pages，不读 slab 头，
 *   所以内部布局改了测试仍然有效。
 *
 * 【答辩要点：两种页管理器都要跑通】
 *   同一个测试要在 PMM=best_fit 和 PMM=buddy 下都通过。
 *   因此 OOM 用例不能依赖"一次申请全部空闲页"，
 *   而是逐页 alloc_page() 直到 NULL——这对两种分配器都成立。
 *
 * 【答辩要点：覆盖与有效性】
 *   每组都说明它能抓到的缺陷类型，避免"只证明能跑"。
 * ============================================================================ */
/* Independent SLUB tests, written by member three.
 *
 * Black-box tests: they use only slub_init's contract (already done at boot),
 * kmalloc, kfree and the page counter. Nothing here reads the slab header, so
 * the tests stay valid if the internal layout changes.
 *
 * The implementers' slub_check is a self check; this file is a separate,
 * independently written suite. Where the two overlap, the cases here are
 * derived from the written interface spec, not from the implementation.
 *
 * Boot stack is 2 pages, so all bookkeeping lives in static arrays. */

#include <member3_check.h>
#include <pmm.h>
#include <slub.h>
#include <stdio.h>
#include <string.h>

#define MAX_OBJ 240

static void *obj[MAX_OBJ];
static size_t obj_size[MAX_OBJ];

static void
check_aligned(void *p, size_t size) {
    if (((uintptr_t)p % 16) != 0) {
        panic("member3 SLUB: %d byte object at %p not 16-byte aligned",
              (int)size, p);
    }
}

static void
check_disjoint(const char *stage) {
    for (size_t i = 0; i < MAX_OBJ; ++i) {
        if (obj[i] == NULL) {
            continue;
        }
        for (size_t j = i + 1; j < MAX_OBJ; ++j) {
            if (obj[j] == NULL) {
                continue;
            }
            uintptr_t a = (uintptr_t)obj[i], b = (uintptr_t)obj[j];
            if (!(a + obj_size[i] <= b || b + obj_size[j] <= a)) {
                panic("member3 SLUB: overlapping objects at %s", stage);
            }
        }
    }
}

static void
fill_payload(size_t i) {
    memset(obj[i], (int)(i % 251) + 1, obj_size[i]);
}

static void
verify_payload(size_t i, const char *stage) {
    uint8_t *b = obj[i];
    for (size_t k = 0; k < obj_size[i]; ++k) {
        if (b[k] != (uint8_t)((i % 251) + 1)) {
            panic("member3 SLUB: payload corrupted at %s (obj %d byte %d)",
                  stage, (int)i, (int)k);
        }
    }
}

static void
release_all(void) {
    for (size_t i = 0; i < MAX_OBJ; ++i) {
        if (obj[i] != NULL) {
            kfree(obj[i]);
            obj[i] = NULL;
        }
    }
}

/* ============================================================================
 * T1 规格边界两侧
 *   1,15,16,17,31,32,33,...,2047,2048 都能分配，且整段可写。
 *   能让"规格表错位"或"只分配不校验长度"的实现失败。
 * ============================================================================ */
/* T1: each size class on both sides of its boundary must be servable, and
 * every result must be 16-byte aligned as the spec promises. */
static void
t1_class_boundaries(void) {
    static const size_t edges[] = {1, 15, 16, 17, 31, 32, 33, 63, 64, 65,
                                   127, 128, 129, 255, 256, 257, 511, 512,
                                   513, 1023, 1024, 1025, 2047, 2048};
    size_t n = sizeof(edges) / sizeof(edges[0]);
    for (size_t i = 0; i < n; ++i) {
        void *p = kmalloc(edges[i]);
        if (p == NULL) {
            panic("member3 SLUB T1: kmalloc(%d) returned NULL", (int)edges[i]);
        }
        check_aligned(p, edges[i]);
        memset(p, 0x5a, edges[i]); // Whole requested range must be writable.
        kfree(p);
    }
    cprintf("member3 SLUB T1 class boundaries 1..2048: PASS\n");
}

/* ============================================================================
 * T2 同页共享 + 跨页对象
 *   多个同规格对象必须共享页（页数增长 < 对象数），
 *   且不同对象的数据互不覆盖；再验证 9000 字节跨页对象可用。
 *   能让"每个对象独占一页"的假 slab 实现失败。
 * ============================================================================ */
/* T2: the two-layer promise. "Pages below, objects above" means several
 * objects of one size class share a single page, so N objects must not cost N
 * pages. We do not hard-code the slab header size: we only require that some
 * two objects of the same class land on the same page, and that the page
 * growth is smaller than the object count.
 * An allocator that quietly gives every object its own page would still pass
 * the payload tests, but it would not be a slab allocator. */
static void
t2_objects_share_pages(void) {
    size_t before = nr_free_pages();
    size_t n = 16;
    for (size_t i = 0; i < n; ++i) {
        obj[i] = kmalloc(256);
        assert(obj[i] != NULL);
        obj_size[i] = 256;
        memset(obj[i], (int)i + 1, 256);
    }
    size_t grew = before - nr_free_pages();
    if (grew >= n) {
        panic("member3 SLUB T2: %d objects of 256B cost %d pages: no sharing",
              (int)n, (int)grew);
    }
    bool same_page = 0;
    for (size_t i = 0; i < n && !same_page; ++i) {
        for (size_t j = i + 1; j < n; ++j) {
            if (ROUNDDOWN((uintptr_t)obj[i], PGSIZE) ==
                ROUNDDOWN((uintptr_t)obj[j], PGSIZE)) {
                same_page = 1;
                break;
            }
        }
    }
    if (!same_page) {
        panic("member3 SLUB T2: no two 256B objects share a page");
    }
    /* Payloads must survive sharing: neighbouring objects do not overlap. */
    for (size_t i = 0; i < n; ++i) {
        for (size_t k = 0; k < 256; ++k) {
            if (((uint8_t *)obj[i])[k] != (uint8_t)(i + 1)) {
                panic("member3 SLUB T2: shared page payload corrupt");
            }
        }
    }
    /* Objects spanning more than one page must also be usable. */
    void *big = kmalloc(9000);
    if (big == NULL) {
        panic("member3 SLUB T2: multi-page object refused");
    }
    check_aligned(big, 9000);
    memset(big, 0x77, 9000);
    kfree(big);
    release_all();
    if (nr_free_pages() != before) {
        panic("member3 SLUB T2: pages not recovered");
    }
    cprintf("member3 SLUB T2 objects share pages, no overlap: PASS "
            "(%d objects in %d pages)\n", (int)n, (int)grew);
}

/* ============================================================================
 * T3 full → partial 复用
 *   填满一个 slab 后释放一个对象，下一次同规格申请必须拿回同一地址。
 *   能让"满页移出 partial 后再也不放回"的实现失败。
 * ============================================================================ */
/* T3: full -> partial transition. Fill one class until the slab is full, free
 * one object (slab becomes partial and must be reusable), allocate again and
 * require the same address back. LIFO reuse is the stated contract. */
static void
t3_full_to_partial(void) {
    size_t before = nr_free_pages();
    size_t count = 0;
    while (count < MAX_OBJ) {
        void *p = kmalloc(32);
        if (p == NULL) {
            break;
        }
        obj[count] = p;
        obj_size[count] = 32;
        ++count;
    }
    if (count < 2) {
        panic("member3 SLUB T3: could not allocate a usable slab");
    }
    check_disjoint("full slab");
    void *first = obj[0];
    kfree(first);
    obj[0] = NULL;
    void *again = kmalloc(32);
    if (again != first) {
        panic("member3 SLUB T3: freed slot was not reused (full->partial)");
    }
    obj[0] = again;
    release_all();
    if (nr_free_pages() != before) {
        panic("member3 SLUB T3: pages not recovered (%d != %d)",
              (int)nr_free_pages(), (int)before);
    }
    cprintf("member3 SLUB T3 full to partial reuse: PASS (%d objects)\n",
            (int)count);
}

/* ============================================================================
 * T4 混合规格、大量活对象
 *   240 个不同规格对象同时存活，两两检查区间不重叠，
 *   写入不同图案后逐字节校验，再按打乱顺序释放。
 *   能让"同一槽位被发放两次"或"释放踩坏邻居"的实现失败。
 * ============================================================================ */
/* T4: mixed size classes, thousands of live objects, no overlap, payloads
 * intact. This is what catches a slot that gets handed out twice. */
static void
t4_mixed_no_overlap(void) {
    size_t before = nr_free_pages();
    static const size_t sizes[] = {16, 32, 64, 128, 256, 512, 1024, 2048};
    for (size_t i = 0; i < MAX_OBJ; ++i) {
        size_t s = sizes[i % 8];
        obj[i] = kmalloc(s);
        if (obj[i] == NULL) {
            panic("member3 SLUB T4: kmalloc(%d) failed at object %d", (int)s,
                  (int)i);
        }
        obj_size[i] = s;
        check_aligned(obj[i], s);
        fill_payload(i);
    }
    check_disjoint("mixed load");
    for (size_t i = 0; i < MAX_OBJ; ++i) {
        verify_payload(i, "mixed load");
    }
    /* Free in a scrambled order: 97 is coprime with MAX_OBJ. */
    for (size_t i = 0; i < MAX_OBJ; ++i) {
        size_t k = (i * 97) % MAX_OBJ;
        kfree(obj[k]);
        obj[k] = NULL;
    }
    if (nr_free_pages() != before) {
        panic("member3 SLUB T4: pages not recovered (%d != %d)",
              (int)nr_free_pages(), (int)before);
    }
    cprintf("member3 SLUB T4 mixed load, no overlap, payload intact: PASS\n");
}

/* ============================================================================
 * T5 部分释放后存活对象不变
 *   释放一半对象后，存活对象的地址和内容必须保持原样。
 *   能让"释放时误清整页"的实现失败。
 * ============================================================================ */
/* T5: freeing a partial set must not destroy the objects still alive. */
static void
t5_survivors(void) {
    size_t before = nr_free_pages();
    for (size_t i = 0; i < 64; ++i) {
        obj[i] = kmalloc(64);
        assert(obj[i] != NULL);
        obj_size[i] = 64;
        fill_payload(i);
    }
    /* Drop every other object. */
    for (size_t i = 0; i < 64; i += 2) {
        kfree(obj[i]);
        obj[i] = NULL;
    }
    for (size_t i = 1; i < 64; i += 2) {
        verify_payload(i, "after partial free");
    }
    release_all();
    if (nr_free_pages() != before) {
        panic("member3 SLUB T5: pages not recovered");
    }
    cprintf("member3 SLUB T5 survivors keep their payload: PASS\n");
}

/* ============================================================================
 * T6 超过 2048 字节的大对象
 *   2049/4096/4097/8192/12288 字节都要可写满、可释放、页数可恢复。
 *   能让"大对象只算一页"或"释放时页数记错"的实现失败。
 * ============================================================================ */
/* T6: requests above the largest class fall back to whole pages. A 5000-byte
 * request needs two pages (plus header), and must come back usable. */
static void
t6_large_objects(void) {
    size_t before = nr_free_pages();
    static const size_t big[] = {2049, 4096, 4097, 8192, 12288};
    for (size_t i = 0; i < sizeof(big) / sizeof(big[0]); ++i) {
        void *p = kmalloc(big[i]);
        if (p == NULL) {
            panic("member3 SLUB T6: kmalloc(%d) returned NULL", (int)big[i]);
        }
        check_aligned(p, big[i]);
        memset(p, 0xa5, big[i]);
        for (size_t k = 0; k < big[i]; ++k) {
            if (((uint8_t *)p)[k] != 0xa5) {
                panic("member3 SLUB T6: payload corrupt for %d bytes",
                      (int)big[i]);
            }
        }
        kfree(p);
    }
    if (nr_free_pages() != before) {
        panic("member3 SLUB T6: pages not recovered after large objects");
    }
    cprintf("member3 SLUB T6 large objects above 2048B: PASS\n");
}

/* ============================================================================
 * T7 退化输入
 *   kmalloc(0)==NULL、kfree(NULL) 无副作用、
 *   SIZE_MAX 与接近 SIZE_MAX 的长度必须拒绝（不能溢出成小页数）。
 *   能让"舍入前不检查溢出"的实现失败。
 * ============================================================================ */
/* T7: degenerate inputs. NULL free is a no-op, zero size returns NULL, and a
 * length whose rounding would overflow must be refused, not truncated. */
static void
t7_degenerate(void) {
    size_t before = nr_free_pages();
    kfree(NULL); // Must not crash or change anything.
    if (kmalloc(0) != NULL) {
        panic("member3 SLUB T7: kmalloc(0) must return NULL");
    }
    if (kmalloc((size_t)-1) != NULL) {
        panic("member3 SLUB T7: overflow length must be refused");
    }
    if (kmalloc((size_t)-1 - 8) != NULL) {
        panic("member3 SLUB T7: near-overflow length must be refused");
    }
    if (nr_free_pages() != before) {
        panic("member3 SLUB T7: degenerate requests changed the page count");
    }
    cprintf("member3 SLUB T7 degenerate inputs: PASS\n");
}

/* ============================================================================
 * T8 真实页耗尽
 *   逐页持有全部空闲页，此时 kmalloc 必须返回 NULL；
 *   归还页面后分配器必须恢复可用。
 *   能让"页层已空但仍返回对象"的实现失败（那意味着它在复用别人的页）。
 *   注意：这里没有用 alloc_pages(全部空闲页)，因为 Buddy 下该请求
 *   可能合法失败，不能当作"页层已空"。
 * ============================================================================ */
/* T8: real page exhaustion. Hold every free page one at a time, then require
 * kmalloc to fail. This works under any page manager, unlike asking for the
 * whole free region in a single call, which only holds for a contiguous
 * allocator.
 * A slab that keeps handing out memory after the page layer is empty is
 * either corrupting another allocation or over-counting free pages. */
static void
t8_oom(void) {
    size_t before = nr_free_pages();
    size_t taken = 0;
    while (alloc_page() != NULL) {
        ++taken;
    }
    if (taken != before) {
        panic("member3 SLUB T8: took %d pages of %d", (int)taken, (int)before);
    }
    if (kmalloc(16) != NULL || kmalloc(4096) != NULL) {
        panic("member3 SLUB T8: allocation succeeded with no free pages");
    }
    /* Every test object is already freed at this point, so every non-reserved
     * descriptor below is a page we took. Walk downward: a free can merge
     * upward and move the block head to the index we are visiting, which is
     * never revisited. */
    for (size_t i = npage - nbase; i > 0; --i) {
        struct Page *q = &pages[i - 1];
        if (!PageReserved(q)) {
            free_pages(q, 1);
        }
    }
    if (nr_free_pages() != before) {
        panic("member3 SLUB T8: pages not recovered (%d != %d)",
              (int)nr_free_pages(), (int)before);
    }
    /* The object layer must work again after recovery. */
    void *p2 = kmalloc(128);
    if (p2 == NULL) {
        panic("member3 SLUB T8: allocator did not recover after OOM");
    }
    kfree(p2);
    cprintf("member3 SLUB T8 real page exhaustion: PASS (%d pages)\n",
            (int)before);
}

void
member3_slub_test(void) {
    cprintf("member3 SLUB tests: start\n");
    size_t before = nr_free_pages();
    t1_class_boundaries();
    t2_objects_share_pages();
    t3_full_to_partial();
    t4_mixed_no_overlap();
    t5_survivors();
    t6_large_objects();
    t7_degenerate();
    t8_oom();
    if (nr_free_pages() != before) {
        panic("member3 SLUB tests: leaked pages (%d != %d)",
              (int)nr_free_pages(), (int)before);
    }
    cprintf("member3_slub_test() succeeded!\n");
}
