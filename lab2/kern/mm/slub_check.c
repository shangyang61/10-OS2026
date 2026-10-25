/*
 * ============================================================================
 * kern/mm/slub_check.c —— 成员二的 SLUB 实现者自检
 * ============================================================================
 * 【它覆盖什么】
 *   - 0 长度、NULL、SIZE_MAX、超过物理内存的长度；
 *   - 规格边界、同页多对象、跨页；
 *   - 所有活对象区间两两不重叠；
 *   - 写入后逐字节校验数据保持；
 *   - 从满页释放一个对象后下一次申请复用该槽位；
 *   - 乱序释放后空闲页数恢复。
 *
 * 【答辩要点：OOM 的两种写法】
 *   Best-Fit 下可以 alloc_pages(before) 一次拿走全部空闲页；
 *   Buddy 下空闲区跨多个阶，这个请求可能合法失败，
 *   所以改用"逐页 alloc_page 直到 NULL"。
 *   两种写法都只是把页层清空，目的相同。
 *
 * 【它和成员三测试的关系】
 *   这是实现者自检，不代替 member3_slub_test.c 的独立测试。
 * ============================================================================ */
#include <slub.h>
#include <pmm.h>
#include <string.h>
#include <stdio.h>

// Small stack footprint: boot stack is only two pages.
static void *objects[300];
static size_t sizes[300];

void slub_check(void) {
    const size_t boundaries[] = {
        1, 15, 16, 17, 31, 32, 33, 63, 64, 65, 127, 128, 129,
        255, 256, 257, 511, 512, 513, 1023, 1024, 1025,
        2047, 2048, 2049, 4095, 4096, 4097, 8192
    };
    size_t before = nr_free_pages();
    assert(kmalloc(0) == NULL);
    kfree(NULL);
    assert(kmalloc((size_t)-1) == NULL);
    assert(kmalloc((npage - nbase + 1) * PGSIZE) == NULL);
    /* OOM by page exhaustion. Best-Fit keeps the whole free region as one
     * contiguous block, so a single request can take it all. Buddy splits its
     * free space over several orders, so that same call may legitimately fail;
     * there we take one page at a time instead. Either way the page layer is
     * empty afterwards, which is what this check needs. */
#if defined(USE_BUDDY_PMM)
    size_t taken = 0;
    while (alloc_page() != NULL) {
        ++taken;
    }
    assert(taken == before); // Single pages must exhaust the machine.
#else
    struct Page *all = alloc_pages(before);
    assert(all != NULL);
#endif
    assert(kmalloc(16) == NULL && kmalloc(4096) == NULL);
#if defined(USE_BUDDY_PMM)
    /* No SLUB object is live yet, so every non-reserved descriptor is a page
     * we just took. Walk downward: a free may merge upward and move the block
     * head to the current index, which we never revisit. */
    for (size_t i = npage - nbase; i > 0; --i) {
        struct Page *q = &pages[i - 1];
        if (!PageReserved(q)) {
            free_pages(q, 1);
        }
    }
#else
    free_pages(all, before);
#endif
    assert(nr_free_pages() == before);
    for (size_t i = 0; i < 300; ++i) {
        sizes[i] = i < 260 ? 16 :
                   boundaries[(i - 260) % (sizeof(boundaries) / sizeof(size_t))];
        objects[i] = kmalloc(sizes[i]);
        assert(objects[i] != NULL && (uintptr_t)objects[i] % 16 == 0);
        for (size_t j = 0; j < i; ++j) {
            uintptr_t a = (uintptr_t)objects[i], b = (uintptr_t)objects[j];
            assert(a + sizes[i] <= b || b + sizes[j] <= a);
        }
        memset(objects[i], (i % 251) + 1, sizes[i]);
    }
    // Fill multiple slabs, then visit all live payloads before freeing any.
    for (size_t i = 0; i < 300; ++i) {
        for (size_t j = 0; j < sizes[i]; ++j) {
            assert(((uint8_t *)objects[i])[j] == (i % 251) + 1);
        }
    }
    void *reused = objects[0];
    kfree(reused);
    objects[0] = kmalloc(16); // A formerly full slab becomes partial and reusable.
    assert(objects[0] == reused);
    // 137 is coprime to 300: each allocation is freed exactly once.
    for (size_t i = 0; i < 300; ++i) {
        kfree(objects[(i * 137) % 300]);
    }
    assert(nr_free_pages() == before);
    cprintf("slub_check() succeeded! (boundaries, reuse, payloads, page recovery)\n");
}
