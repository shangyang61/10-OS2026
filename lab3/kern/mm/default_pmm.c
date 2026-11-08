/*
 * ============================================================================
 * 【代码归属】课程实验框架提供完整 First-Fit 实现；成员二修复了一处真实缺陷
 * 【判定依据】按 lab2.docx 的任务划分与代码内容判断，不依赖可能被清除的 lab2 / YOUR CODE 标记。
 * ============================================================================
 * 【答辩可以直接这样念】
 *
 *   “First-Fit 是课程框架给的完整实现，不是我们写的。
 *    它的空闲链表按物理地址排序，只挂空闲块的头页，头页 property 记块长。
 *    申请 n 页时从低地址往高地址找，第一个长度 >= n 的块就切出 n 页，
 *    剩下的部分在 base+n 处重新建头。释放时先并前块、再并后块，
 *    判断用的是 prev + prev->property == base 这种区间关系，
 *    而不是链表相邻。
 *    指导书给出的这份实现里，两处尾部插入都少了 break，插入新尾节点后
 *    循环还会继续访问它，导致重复插入、双向链接被破坏。我们的贡献是
 *    定位并修复了这个缺陷，并用 tests/reproduce_first_fit_bug.py
 *    复现旧版本的断言失败、再验证修复后通过。”

 * ============================================================================
 * 【本组改动】成员二在 init_memmap 和 free_pages 的尾部插入分支各补一个 break；两个框架测试函数未改。
 * ============================================================================
 */
/*
 * ============================================================================
 * kern/mm/default_pmm.c —— First-Fit（首次适配）页分配器
 * ============================================================================
 * 【算法一句话】
 *   空闲块按物理地址从小到大排成一条链，申请 n 页时从低地址往高地址找，
 *   遇到的第一个长度 >= n 的块就切出 n 页，剩下的部分放回链表。
 *
 * 【答辩要点一：链表里放的是什么】
 *   只放"空闲块的头页"。头页的 property 记录这个块有多少页，
 *   PG_property 置位。块内其余页不在链表上，也不带 PG_property。
 *   所以 le2page 取出来的 p 是块头，p + p->property 是块尾之后的位置。
 *
 * 【答辩要点二：为什么按地址排序】
 *   释放时要判断"前后物理邻居能不能合并"。链表按地址有序，
 *   就只需要看 prev 和 next 两个节点，不必遍历整个链表找邻居。
 *
 * 【答辩要点三：本文件修过的真实缺陷】
 *   尾部插入分支原来少了 break：把新块插到链表末尾后，
 *   循环还会继续检查新插入的节点，导致重复插入、双向链接被破坏。
 *   现在两个插入分支都显式 break，测试
 *   tests/reproduce_first_fit_bug.py 能复现旧缺陷、验证修复。
 *
 * 【答辩要点四：与 Best-Fit 的区别】
 *   First-Fit 找到第一个够大的就停，不比较剩余空间大小；
 *   Best-Fit 要遍历完整条链，选"最小的够用块"，减少大块被切碎。
 *   两者回收和合并逻辑相同。
 *
 * 注意：basic_check 与 default_check 是框架保护函数，函数体不能改，
 * 连内部注释都不要动，否则函数级 SHA256 校验会失败。
 * ============================================================================
 */
#include <pmm.h>
#include <list.h>
#include <string.h>
#include <default_pmm.h>

/* First-Fit 只用一条空闲链。
 * free_area.nr_free 记录空闲页总数（页，不是块数）。 */
static free_area_t free_area;

#define free_list (free_area.free_list)
#define nr_free (free_area.nr_free)

/* 初始化：链表哨兵 + 计数清零。此时还没有任何可用内存登记。 */
static void
default_init(void) {
    list_init(&free_list);
    nr_free = 0;
}

/* *
 * default_init_memmap —— 把 page_init 交来的一段连续可用页登记成空闲块
 *
 *   1. 断言这些页当前是 Reserved（page_init 的默认状态）；
 *   2. 清掉 flags/property/ref，表示"从此可分配"；
 *   3. 只用块头记录长度 n 并置 PG_property；
 *   4. 按地址顺序插进链表，保持有序。
 *
 *   注意：这里传入的通常是一整段连续空闲区，所以只建一个块头。
 * */
static void
default_init_memmap(struct Page *base, size_t n) {
    assert(n > 0);
    struct Page *p = base;
    for (; p != base + n; p ++) {
        assert(PageReserved(p));
        p->flags = p->property = 0;
        set_page_ref(p, 0);
    }
    base->property = n;
    SetPageProperty(base);
    nr_free += n;
    if (list_empty(&free_list)) {
        list_add(&free_list, &(base->page_link));
    } else {
        list_entry_t* le = &free_list;
        while ((le = list_next(le)) != &free_list) {
            struct Page* page = le2page(le, page_link);
            if (base < page) {
                list_add_before(le, &(base->page_link));
                break;
            } else if (list_next(le) == &free_list) {
                list_add(le, &(base->page_link));
                break; // 插到尾部后必须停，否则会重复访问/插入新节点
            }
        }
    }
}

/* *
 * default_alloc_pages —— First-Fit 分配
 *
 *   【答辩要点】"首次适配"的含义就是找到第一个够大的块立即返回，
 *   不继续找更合适的。所以它是 O(k)，k 是从低地址数到第一个可用块的距离。
 *
 *   切分：若块长 > n，把前 n 页交付，剩余部分在 base+n 处重新建头，
 *   插回原来的位置（list_add(prev, ...) 正好保持有序）。
 *   计数只减 n，不是减整块。
 * */
static struct Page *
default_alloc_pages(size_t n) {
    assert(n > 0);
    if (n > nr_free) {
        return NULL; // 总数都不够，直接失败
    }
    struct Page *page = NULL;
    list_entry_t *le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        if (p->property >= n) {
            page = p;
            break; // First-Fit：第一个够大的就用
        }
    }
    if (page != NULL) {
        list_entry_t* prev = list_prev(&(page->page_link));
        list_del(&(page->page_link));
        if (page->property > n) {
            struct Page *p = page + n;
            p->property = page->property - n;
            SetPageProperty(p);
            list_add(prev, &(p->page_link));
        }
        nr_free -= n;
        ClearPageProperty(page); // 交付的块不再是空闲块头
    }
    return page;
}

/* *
 * default_free_pages —— 归还并合并
 *
 *   【答辩要点一：判断邻居用的是区间关系，不是链表相邻】
 *     prev + prev->property == base     前块紧挨着我
 *     base + base->property  == next    我紧挨着后块
 *   链表相邻只说明地址次序，不说明物理相邻，必须做这个等式判断。
 *
 *   【答辩要点二：双向合并的顺序】
 *   先并前块，此时 base 要更新成前块头，再判断后块；
 *   否则后块判断会用错起点。合并只改描述符和链表，不搬数据。
 *
 *   【答辩要点三：计数只加一次】
 *   进入函数时 nr_free += n；后面合并只是把两个空闲块并成一个，
 *   不重复增加空闲页数。
 * */
static void
default_free_pages(struct Page *base, size_t n) {
    assert(n > 0);
    struct Page *p = base;
    for (; p != base + n; p ++) {
        assert(!PageReserved(p) && !PageProperty(p));
        p->flags = 0;
        set_page_ref(p, 0);
    }
    base->property = n;
    SetPageProperty(base);
    nr_free += n;

    if (list_empty(&free_list)) {
        list_add(&free_list, &(base->page_link));
    } else {
        list_entry_t* le = &free_list;
        while ((le = list_next(le)) != &free_list) {
            struct Page* page = le2page(le, page_link);
            if (base < page) {
                list_add_before(le, &(base->page_link));
                break;
            } else if (list_next(le) == &free_list) {
                list_add(le, &(base->page_link));
                break; // 记录真正的后继，供后面判断后邻居
            }
        }
    }

    /* 先看前邻居：若物理相接，合并到前块头。 */
    list_entry_t* le = list_prev(&(base->page_link));
    if (le != &free_list) {
        p = le2page(le, page_link);
        if (p + p->property == base) {
            p->property += base->property;
            ClearPageProperty(base);
            list_del(&(base->page_link));
            base = p;
        }
    }

    /* 再看后邻居：用更新后的 base 和长度判断。 */
    le = list_next(&(base->page_link));
    if (le != &free_list) {
        p = le2page(le, page_link);
        if (base + base->property == p) {
            base->property += p->property;
            ClearPageProperty(p);
            list_del(&(p->page_link));
        }
    }
}

/* 返回空闲页总数。 */
static size_t
default_nr_free_pages(void) {
    return nr_free;
}

/* ===== 以下是框架保护测试，函数体不得修改 =====
 * basic_check：分配/释放三个单页，检查计数与链表基本行为。
 * default_check：在 basic_check 基础上检查切分、合并、耗尽。
 * 保护原因：这些测试是评分与回归的共同基准，改了就无法证明实现正确。
 */
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

// LAB2: below code is used to check the first fit allocation algorithm (your EXERCISE 1) 
/* NOTICE: You SHOULD NOT CHANGE basic_check, default_check functions!
 *
 * 【答辩要点】这两个函数由框架提供，是"裁判"，不是"选手"。
 * 实现者如果为了通过而改测试，等于自己改评分标准。
 * 保护清单用函数级 SHA256 固定它们，任何修改都会被 check_preserved 抓到。 */
static void
default_check(void) {
    int count = 0, total = 0;
    list_entry_t *le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        assert(PageProperty(p));
        count ++, total += p->property;
    }
    assert(total == nr_free_pages());

    basic_check();

    struct Page *p0 = alloc_pages(5), *p1, *p2;
    assert(p0 != NULL);
    assert(!PageProperty(p0));

    list_entry_t free_list_store = free_list;
    list_init(&free_list);
    assert(list_empty(&free_list));
    assert(alloc_page() == NULL);

    unsigned int nr_free_store = nr_free;
    nr_free = 0;

    free_pages(p0 + 2, 3);
    assert(alloc_pages(4) == NULL);
    assert(PageProperty(p0 + 2) && p0[2].property == 3);
    assert((p1 = alloc_pages(3)) != NULL);
    assert(alloc_page() == NULL);
    assert(p0 + 2 == p1);

    p2 = p0 + 1;
    free_page(p0);
    free_pages(p1, 3);
    assert(PageProperty(p0) && p0->property == 1);
    assert(PageProperty(p1) && p1->property == 3);

    assert((p0 = alloc_page()) == p2 - 1);
    free_page(p0);
    assert((p0 = alloc_pages(2)) == p2 + 1);

    free_pages(p0, 2);
    free_page(p2);

    assert((p0 = alloc_pages(5)) != NULL);
    assert(alloc_page() == NULL);

    assert(nr_free == 0);
    nr_free = nr_free_store;

    free_list = free_list_store;
    free_pages(p0, 5);

    le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        count --, total -= p->property;
    }
    assert(count == 0);
    assert(total == 0);
}

const struct pmm_manager default_pmm_manager = {
    .name = "default_pmm_manager",
    .init = default_init,
    .init_memmap = default_init_memmap,
    .alloc_pages = default_alloc_pages,
    .free_pages = default_free_pages,
    .nr_free_pages = default_nr_free_pages,
    .check = default_check,
};
