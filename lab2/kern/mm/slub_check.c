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
