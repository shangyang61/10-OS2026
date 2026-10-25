/*
 * ============================================================================
 * kern/mm/slub.c —— 简化 SLUB 对象分配器（成员二，答辩重点）
 * ============================================================================
 * 【一句话原理】
 *   页分配器按 4 KiB 页工作，但内核里大量对象只有几十字节。
 *   SLUB 在页之上再加一层：一页切成若干个等长槽位，
 *   小对象共享同一页，避免"申请 16 字节却独占 4 KiB"的浪费。
 *
 * 【答辩要点一：两层结构】
 *      kmalloc/kfree          ← 对象层（本文件）
 *            ↓
 *      alloc_pages/free_pages ← 页层（Best-Fit / Buddy）
 *
 *   SLUB 只通过 alloc_pages/free_pages 访问物理页，
 *   不依赖 Best-Fit 的私有链表，所以换页管理器它照样能工作。
 *   这正是"底层按页、上层按对象"的分层设计。
 *
 * 【答辩要点二：八种规格】
 *   16、32、64、128、256、512、1024、2048 字节。
 *   申请 n 字节时，向上找第一个 >= n 的规格（内部碎片换取管理简单）。
 *   超过 2048 字节不走 slab，直接申请连续多页。
 *
 * 【答辩要点三：slab 头与 freelist】
 *   每页开头是 struct slab 元数据（magic、规格号、容量、已用数、
 *   空闲链表指针、链表节点、占用位图），按 16 字节对齐。
 *   空闲槽位内部存"下一个空闲槽位"的指针，形成 freelist；
 *   分配时取头节点，释放时头插。
 *   位图记录每个槽位是否已分配，用来做有限的一致性检查。
 *
 * 【答辩要点四：full / partial / empty 三态】
 *   - partial 链表里只放"没满"的 slab；
 *   - 一个 slab 分满后从 partial 摘掉（full）；
 *   - 释放一个对象后若由满变未满，重新挂回 partial；
 *   - 最后一个对象释放后，整页还给页分配器（empty）。
 *   这样空闲页不会被长期占着。
 *
 * 【答辩要点五：大对象】
 *   >2048 字节时按 ceil((size + header) / PGSIZE) 申请连续页，
 *   头部记录 npages，释放时用同一个 npages 调 free_pages。
 *   Buddy 可能向上取整给更大的块，但页层会在块首记录真实阶数，
 *   所以这里传"请求页数"仍然能完整归还。
 *
 * 【答辩要点六：限制】
 *   单 hart 教学实现，没有锁、每 CPU 缓存、NUMA、回收压力处理；
 *   魔数和位图只提供有限诊断，不能保证任意野指针安全；
 *   返回内存不保证清零，只保证 16 字节对齐。
 * ============================================================================
 */
#include <slub.h>
#include <pmm.h>
#include <list.h>
#include <string.h>
#include <stdio.h>

/* slab 头的魔数：用来区分普通 slab 与大对象页头，也能发现明显越界改写。 */
#define SLUB_MAGIC 0x534c5542534d414cULL
#define LARGE_MAGIC 0x534c55424c415247ULL
#define ALIGNMENT 16
#define CACHE_COUNT 8
#define BITMAP_BYTES 32

/* *
 * struct slab —— 每页开头的元数据
 *
 *   magic       魔数（SLUB_MAGIC 或 LARGE_MAGIC）
 *   cache_index 属于哪个规格（0..7）
 *   capacity    本页能放多少个对象
 *   inuse       已分配对象数
 *   npages      大对象用了多少页；小 slab 恒为 1
 *   freelist    空闲槽位链表的头
 *   link        挂到 partial 链表上的节点
 *   allocated   占用位图，每个槽位一位
 *
 * 这个结构必须放在页内、且不能与对象区重叠，所以下面用 header_size()
 * 向上对齐到 16 字节。
 * */
struct slab {
    uint64_t magic;
    size_t cache_index, capacity, inuse, npages;
    void *freelist;
    list_entry_t link; // Only partial slabs are on the cache list.
    uint8_t allocated[BITMAP_BYTES];
};

/* 每种规格一个缓存：object_size + partial 链表头。 */
struct object_cache {
    size_t object_size;
    list_entry_t partial;
};

static struct object_cache caches[CACHE_COUNT];
static bool initialized;

/* 页头大小：sizeof(struct slab) 向上取整到 16 字节。
 * 答辩时如果被问"一页能放几个对象"，就要用
 * (PGSIZE - header_size()) / object_size 来算。 */
static size_t header_size(void) {
    return ROUNDUP(sizeof(struct slab), ALIGNMENT);
}

/* struct Page* → 该页的内核虚拟地址。
 * 物理地址来自 page2pa，加上直接映射偏移就是可访问的虚拟地址。
 * 注意：不能把 struct Page 本身当成数据页来写！ */
static void *page_address(struct Page *p) {
    return (void *)(page2pa(p) + va_pa_offset);
}

/* 初始化八个规格的 partial 链表。只允许调用一次，重复初始化会丢页。 */
void slub_init(void) {
    assert(!initialized); // Reinitializing live caches would leak their pages.
    for (size_t i = 0; i < CACHE_COUNT; ++i) {
        caches[i].object_size = (size_t)16 << i; // 16,32,...,2048
        list_init(&caches[i].partial);
    }
    initialized = 1;
}

/* *
 * new_slab —— 为一个规格新申请一页并切成槽位
 *
 *   1. alloc_page() 拿一页；
 *   2. 清空页头，写元数据；
 *   3. 算出容量 capacity；
 *   4. 把所有槽位用 freelist 串起来（这里从后往前串，
 *      所以第一次分配拿到的是最前面的槽位，便于调试）；
 *   5. 挂进 partial 链表。
 *
 *   失败（OOM）返回 NULL，由 kmalloc 转成 NULL。
 * */
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

/* *
 * kmalloc —— 对象层分配入口
 *
 *   0 长度返回 NULL（没有"零字节对象"这种东西）。
 *   大于 2048 字节走大对象路径：直接按页申请。
 *   否则选规格、从 partial 取一个槽位。
 * */
void *kmalloc(size_t size) {
    assert(initialized);
    if (size == 0) {
        return NULL;
    }
    if (size > caches[CACHE_COUNT - 1].object_size) {
        /* 大对象：页数 = ceil((size + 页头) / PGSIZE)。
         * 先检查加法/向上取整会不会溢出，避免算出很小的页数。 */
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
    /* 找到第一个装得下 size 的规格。 */
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
    /* 由对象地址反推槽位号，置位图。 */
    size_t slot = ((char *)obj - (char *)s - header_size()) /
                  caches[index].object_size;
    assert(!(s->allocated[slot / 8] & (1U << (slot % 8))));
    s->allocated[slot / 8] |= 1U << (slot % 8);
    /* 分满后从 partial 摘掉：它已经不能再满足同规格请求。 */
    if (++s->inuse == s->capacity) {
        list_del_init(&s->link);
    }
    return obj;
}

/* *
 * kfree —— 对象层释放入口
 *
 *   NULL 直接返回（和 free(NULL) 一样的约定）。
 *   先看页头魔数判断是大对象还是普通 slab：
 *
 *   大对象：清魔数，按记录的 npages 还页。
 *   普通对象：
 *     1. 校验指针落在对象区、按 stride 对齐、槽位号合法；
 *     2. 清位图，把槽位头插回 freelist；
 *     3. inuse--；
 *     4. 若从"满"变"未满"，挂回 partial；
 *     5. 若 inuse 归零，清魔数并把整页还给页分配器。
 * */
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
    *(void **)ptr = s->freelist; // 释放的槽位重新成为 freelist 头
    s->freelist = ptr;
    --s->inuse;
    if (was_full) {
        list_add(&caches[s->cache_index].partial, &s->link);
    }
    if (s->inuse == 0) {
        /* 全空：立即还页，避免空闲 slab 长期占着物理页。 */
        list_del_init(&s->link);
        s->magic = 0;
        free_page(pa2page(PADDR(s)));
    }
}
