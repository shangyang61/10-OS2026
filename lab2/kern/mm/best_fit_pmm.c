/*
 * ============================================================================
 * kern/mm/best_fit_pmm.c —— Best-Fit（最佳适配）页分配器
 * ============================================================================
 * 【算法一句话】
 *   遍历整条空闲链，在所有"长度 >= n"的块里选最小的那个；
 *   长度相同时选物理地址更低的，减少大块被无谓切碎。
 *
 * 【答辩要点一：与 First-Fit 的对比】
 *   First-Fit 找到第一个够大的就停，实现简单但可能把一个大块切碎，
 *   留下很多小碎片。
 *   Best-Fit 多花一次遍历，换取更少的剩余碎片；
 *   但它的缺点是要扫完整条链，而且会产生很多"很小、几乎用不上"的碎块
 *   （外部碎片问题并没有被消除，只是换了形态）。
 *
 * 【答辩要点二：exact fit 提前退出】
 *   如果已经找到长度恰好等于 n 的块，就不可能更好了，直接 break。
 *   这是一个不影响正确性、但能少扫一段链的优化。
 *
 * 【答辩要点三：不变量】
 *   1. 只有块头在链表上，property 就是块长；
 *   2. 链表按物理地址升序；
 *   3. 相邻空闲块永远已经合并，所以链上不会出现两个物理相邻的节点；
 *   4. nr_free 是空闲页总数，不是块数。
 *
 * 【答辩要点四：回收时的合并】
 *   和 First-Fit 一样，先并前块、再并后块；判断用
 *   prev + prev->property == base 这种区间关系，而不是"链表相邻"。
 *
 * 注意：basic_check 与 best_fit_check 由框架提供，函数体不能改。
 * ============================================================================
 */
#include <pmm.h>
#include <list.h>
#include <best_fit_pmm.h>
#include <stdio.h>

//lab2 function2-2 2412592
/* Best-Fit 同样只用一条按地址排序的空闲链。
 * 只有块头节点在链表上，property = 块长。 */
// Only block heads belong to the address-ordered free list.
static free_area_t free_area;
#define free_list (free_area.free_list)
#define nr_free (free_area.nr_free)

static void best_fit_check(void);

/* 初始化链表哨兵与计数。 */
static void
best_fit_init(void) {
    list_init(&free_list);
    nr_free = 0;
}

/* *
 * best_fit_insert —— 把空闲块按地址插入，并合并物理邻居
 *
 *   插入位置：从低地址往高地址找，第一个地址更大的节点之前。
 *   插入后：
 *     1) 看前节点是否物理相接（prev + prev->property == base），
 *        相接就并入前块，base 前移到前块头；
 *     2) 再看后节点是否与合并后的块相接，相接就并入后块。
 *
 *   合并只改描述符和链表节点，不复制物理数据；nr_free 由调用者维护，
 *   这里不重复加减。
 * */
// Address order makes physical neighbours easy to find when freeing.
static void
best_fit_insert(struct Page *base) {
    list_entry_t *le = list_next(&free_list);
    while (le != &free_list && le2page(le, page_link) < base) {
        le = list_next(le);
    }
    list_add_before(le, &base->page_link);

    le = list_prev(&base->page_link);
    if (le != &free_list) {
        struct Page *prev = le2page(le, page_link);
        assert(prev + prev->property <= base);
        if (prev + prev->property == base) {
            prev->property += base->property;
            ClearPageProperty(base);
            base->property = 0;
            list_del(&base->page_link);
            base = prev;
        }
    }
    le = list_next(&base->page_link);
    if (le != &free_list) {
        struct Page *next = le2page(le, page_link);
        assert(base + base->property <= next);
        if (base + base->property == next) {
            base->property += next->property;
            ClearPageProperty(next);
            next->property = 0;
            list_del(&next->page_link);
        }
    }
}

/* *
 * best_fit_init_memmap —— 登记一段可用物理页
 *
 *   page_init 交来的页默认都是 Reserved，这里统一清标志、清引用计数，
 *   再把整段作为一个空闲块交给插入函数（插入时可能与已有块合并）。
 * */
static void
best_fit_init_memmap(struct Page *base, size_t n) {
    assert(n > 0);
    for (struct Page *p = base; p != base + n; ++p) {
        assert(PageReserved(p));
        p->flags = p->property = 0;
        set_page_ref(p, 0);
    }
    base->property = n;
    SetPageProperty(base);
    nr_free += n;
    best_fit_insert(base);
}

/* *
 * best_fit_alloc_pages —— 选最小充分块
 *
 *   best 记录目前见过的最小充分块：
 *     - 先按"够用"过滤；
 *     - 再按 property 更小优先（这就是 best fit）；
 *     - 长度相等时保留先遇到的（链表按地址升序，所以就是低地址优先）。
 *
 *   切分：交付前 n 页，剩余的 block_size - n 页在 best+n 处重新建头，
 *   插回 best 原来的前驱之后，保持地址有序。
 * */
static struct Page *
best_fit_alloc_pages(size_t n) {
    assert(n > 0);
    if (n > nr_free) {
        return NULL;
    }
    struct Page *best = NULL;
    list_entry_t *le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        if (p->property >= n &&
            (best == NULL || p->property < best->property)) {
            best = p;
            if (p->property == n) {
                break; // 恰好够用，不可能更优
            }
        }
    }
    if (best == NULL) {
        return NULL; // 总页数够，但被切成碎片，没有单块够用
    }
    size_t block_size = best->property;
    le = list_prev(&best->page_link);
    list_del(&best->page_link);
    if (block_size > n) {
        struct Page *rest = best + n;
        rest->property = block_size - n;
        SetPageProperty(rest);
        list_add(le, &rest->page_link);
    }
    ClearPageProperty(best);
    best->property = 0;
    nr_free -= n;
    return best;
}

/* *
 * best_fit_free_pages —— 归还并合并
 *
 *   先把传入区间重置成"干净页"，再建块头、加计数、按地址插回。
 *   真正的合并发生在 best_fit_insert 里（前后各看一次）。
 * */
static void
best_fit_free_pages(struct Page *base, size_t n) {
    assert(n > 0);
    for (struct Page *p = base; p != base + n; ++p) {
        assert(!PageReserved(p) && !PageProperty(p));
        p->flags = p->property = 0;
        set_page_ref(p, 0);
    }
    base->property = n;
    SetPageProperty(base);
    nr_free += n;
    best_fit_insert(base);
}

/* 空闲页总数。 */
static size_t
best_fit_nr_free_pages(void) {
    return nr_free;
}

/* 把六个方法绑成 pmm_manager，pmm.c 通过指针调用。 */
const struct pmm_manager best_fit_pmm_manager = {
    .name = "best_fit_pmm_manager",
    .init = best_fit_init,
    .init_memmap = best_fit_init_memmap,
    .alloc_pages = best_fit_alloc_pages,
    .free_pages = best_fit_free_pages,
    .nr_free_pages = best_fit_nr_free_pages,
    .check = best_fit_check,
};

/* ===== 以下是框架保护测试，函数体不得修改 =====
 * basic_check：三个单页的基本分配/释放，检查链表与计数。
 * best_fit_check：检查选块、切分、合并、碎片情况下返回 NULL。
 * 保护原因见 default_pmm.c 的同名说明。
 */
// Framework tests: students must not modify basic_check or best_fit_check.
//
// 【答辩要点】best_fit_check 里会构造 "* - - *" 这种碎片布局，
// 验证"总页数够但没有足够长的连续块时 alloc_pages(4) 必须返回 NULL"。
// 这正是外部碎片的定义，也是 Best-Fit 与 First-Fit 都要面对的问题。
static void
basic_check(void) {
    struct Page *p0, *p1, *p2;
    p0 = p1 = p2 = NULL;
    assert((p0 = alloc_page()) != NULL);
    assert((p1 = alloc_page()) != NULL);
    assert((p2 = alloc_page()) != NULL);

    assert(p0 != p1 && p0 != p2 && p1 != p2);
    assert(page_ref(p0) == 0 && page_ref(p1) == 0 && page_ref(p2) == 0);

    assert(page2pa(p0) < npage * PGSIZE);
    assert(page2pa(p1) < npage * PGSIZE);
    assert(page2pa(p2) < npage * PGSIZE);

    list_entry_t free_list_store = free_list;
    list_init(&free_list);
    assert(list_empty(&free_list));

    unsigned int nr_free_store = nr_free;
    nr_free = 0;

    assert(alloc_page() == NULL);

    free_page(p0);
    free_page(p1);
    free_page(p2);
    assert(nr_free == 3);

    assert((p0 = alloc_page()) != NULL);
    assert((p1 = alloc_page()) != NULL);
    assert((p2 = alloc_page()) != NULL);

    assert(alloc_page() == NULL);

    free_page(p0);
    assert(!list_empty(&free_list));

    struct Page *p;
    assert((p = alloc_page()) == p0);
    assert(alloc_page() == NULL);

    assert(nr_free == 0);
    free_list = free_list_store;
    nr_free = nr_free_store;

    free_page(p);
    free_page(p1);
    free_page(p2);
}

static void
best_fit_check(void) {
    int score = 0, sumscore = 6;
    int count = 0, total = 0;
    list_entry_t *le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        assert(PageProperty(p));
        count++, total += p->property;
    }
    assert(total == nr_free_pages());

    basic_check();

    #ifdef ucore_test
    score += 1;
    cprintf("grading: %d / %d points\n", score, sumscore);
    #endif
    struct Page *p0 = alloc_pages(5), *p1, *p2;
    assert(p0 != NULL);
    assert(!PageProperty(p0));

    #ifdef ucore_test
    score += 1;
    cprintf("grading: %d / %d points\n", score, sumscore);
    #endif
    list_entry_t free_list_store = free_list;
    list_init(&free_list);
    assert(list_empty(&free_list));
    assert(alloc_page() == NULL);

    #ifdef ucore_test
    score += 1;
    cprintf("grading: %d / %d points\n", score, sumscore);
    #endif
    unsigned int nr_free_store = nr_free;
    nr_free = 0;

    // * - - * -
    free_pages(p0 + 1, 2);
    free_pages(p0 + 4, 1);
    assert(alloc_pages(4) == NULL);
    assert(PageProperty(p0 + 1) && p0[1].property == 2);
    // * - - * *
    assert((p1 = alloc_pages(1)) != NULL);
    assert(alloc_pages(2) != NULL);
    assert(p0 + 4 == p1);

    #ifdef ucore_test
    score += 1;
    cprintf("grading: %d / %d points\n", score, sumscore);
    #endif
    p2 = p0 + 1;
    free_pages(p0, 5);
    assert((p0 = alloc_pages(5)) != NULL);
    assert(alloc_page() == NULL);

    #ifdef ucore_test
    score += 1;
    cprintf("grading: %d / %d points\n", score, sumscore);
    #endif
    assert(nr_free == 0);
    nr_free = nr_free_store;

    free_list = free_list_store;
    free_pages(p0, 5);

    le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        count--, total -= p->property;
    }
    assert(count == 0);
    assert(total == 0);
    #ifdef ucore_test
    score += 1;
    cprintf("grading: %d / %d points\n", score, sumscore);
    #endif
}
