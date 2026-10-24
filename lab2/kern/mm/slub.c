#include <slub.h>
#include <pmm.h>
#include <list.h>
#include <string.h>
#include <stdio.h>

#define SLUB_MAGIC 0x534c5542534d414cULL
#define LARGE_MAGIC 0x534c55424c415247ULL
#define ALIGNMENT 16
#define CACHE_COUNT 8
#define BITMAP_BYTES 32

struct slab {
    uint64_t magic;
    size_t cache_index, capacity, inuse, npages;
    void *freelist;
    list_entry_t link; // Only partial slabs are on the cache list.
    uint8_t allocated[BITMAP_BYTES];
};

struct object_cache {
    size_t object_size;
    list_entry_t partial;
};

static struct object_cache caches[CACHE_COUNT];
static bool initialized;

static size_t header_size(void) {
    return ROUNDUP(sizeof(struct slab), ALIGNMENT);
}

static void *page_address(struct Page *p) {
    return (void *)(page2pa(p) + va_pa_offset);
}

void slub_init(void) {
    assert(!initialized); // Reinitializing live caches would leak their pages.
    for (size_t i = 0; i < CACHE_COUNT; ++i) {
        caches[i].object_size = (size_t)16 << i;
        list_init(&caches[i].partial);
    }
    initialized = 1;
}

static struct slab *new_slab(size_t index) {
    struct Page *p = alloc_page();
    if (p == NULL) {
        return NULL;
    }
    struct slab *s = page_address(p);
    memset(s, 0, header_size());
    s->magic = SLUB_MAGIC;
    s->cache_index = index;
    s->npages = 1;
    size_t stride = caches[index].object_size;
    s->capacity = (PGSIZE - header_size()) / stride;
    assert(s->capacity > 0 && s->capacity <= BITMAP_BYTES * 8);
    // Each free object stores the address of the next free object.
    for (size_t i = s->capacity; i > 0; --i) {
        void *obj = (char *)s + header_size() + (i - 1) * stride;
        *(void **)obj = s->freelist;
        s->freelist = obj;
    }
    list_add(&caches[index].partial, &s->link);
    return s;
}

void *kmalloc(size_t size) {
    assert(initialized);
    if (size == 0) {
        return NULL;
    }
    if (size > caches[CACHE_COUNT - 1].object_size) {
        // Larger requests use continuous pages and retain their exact page count.
        if (size > (size_t)-1 - header_size() - (PGSIZE - 1)) {
            return NULL;
        }
        size_t npages = (size + header_size() + PGSIZE - 1) / PGSIZE;
        struct Page *p = alloc_pages(npages);
        if (p == NULL) {
            return NULL;
        }
        struct slab *s = page_address(p);
        memset(s, 0, header_size());
        s->magic = LARGE_MAGIC;
        s->npages = npages;
        return (char *)s + header_size();
    }
    size_t index = 0;
    while (caches[index].object_size < size) {
        ++index;
    }
    list_entry_t *head = &caches[index].partial;
    struct slab *s = list_empty(head) ? new_slab(index) :
                     to_struct(head->next, struct slab, link);
    if (s == NULL) {
        return NULL;
    }
    void *obj = s->freelist;
    assert(obj != NULL);
    s->freelist = *(void **)obj;
    size_t slot = ((char *)obj - (char *)s - header_size()) /
                  caches[index].object_size;
    assert(!(s->allocated[slot / 8] & (1U << (slot % 8))));
    s->allocated[slot / 8] |= 1U << (slot % 8);
    if (++s->inuse == s->capacity) {
        list_del_init(&s->link); // Full slabs cannot satisfy another request.
    }
    return obj;
}

void kfree(void *ptr) {
    if (ptr == NULL) {
        return;
    }
    assert(initialized);
    struct slab *s = (struct slab *)ROUNDDOWN((uintptr_t)ptr, PGSIZE);
    if (s->magic == LARGE_MAGIC) {
        assert(ptr == (char *)s + header_size());
        size_t npages = s->npages;
        s->magic = 0;
        free_pages(pa2page(PADDR(s)), npages);
        return;
    }
    assert(s->magic == SLUB_MAGIC && s->cache_index < CACHE_COUNT);
    size_t stride = caches[s->cache_index].object_size;
    uintptr_t start = (uintptr_t)s + header_size();
    assert((uintptr_t)ptr >= start);
    size_t offset = (uintptr_t)ptr - start;
    size_t slot = offset / stride;
    assert(offset % stride == 0 && slot < s->capacity && s->inuse > 0);
    assert(s->allocated[slot / 8] & (1U << (slot % 8)));
    s->allocated[slot / 8] &= ~(1U << (slot % 8));
    bool was_full = s->inuse == s->capacity;
    *(void **)ptr = s->freelist;
    s->freelist = ptr;
    --s->inuse;
    if (was_full) {
        list_add(&caches[s->cache_index].partial, &s->link);
    }
    if (s->inuse == 0) {
        list_del_init(&s->link);
        s->magic = 0;
        free_page(pa2page(PADDR(s))); // Return empty slabs to the page allocator.
    }
}
