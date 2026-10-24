#include <pmm.h>
#include <stdio.h>

// Additional implementation checks; framework checks remain unchanged.
void member2_page_check(void) {
    size_t before = nr_free_pages();
    struct Page *base = alloc_pages(32);
    assert(base != NULL);
    free_pages(base, 8);
    free_pages(base + 9, 5);
    free_pages(base + 15, 5);
    free_pages(base + 21, 3);
    // Choose smallest sufficient block; ties choose lower physical address.
    assert(alloc_pages(4) == base + 9);
    assert(alloc_pages(5) == base + 15);
    assert(alloc_pages(3) == base + 21);
    assert(alloc_pages(1) == base + 13);
    assert(alloc_pages(8) == base);
    assert(nr_free_pages() == before - 32);
    // All 32 pages are allocated now. Release out of order to join both sides.
    free_pages(base, 8);
    free_pages(base + 16, 16);
    free_pages(base + 8, 8);
    assert(nr_free_pages() == before);

    // Isolate the complete free region: enough total pages, no long enough block.
    base = alloc_pages(before);
    assert(base != NULL);
    assert(alloc_page() == NULL);
    free_pages(base, 2);
    free_pages(base + 3, 2);
    assert(nr_free_pages() == 4 && alloc_pages(3) == NULL);
    assert(alloc_pages(2) == base);
    assert(alloc_pages(2) == base + 3);
    free_pages(base, before);
    assert(nr_free_pages() == before);
    cprintf("member2_page_check() succeeded! (best choice, ties, merge, fragmentation)\n");
}
