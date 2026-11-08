/*
 * ============================================================================
 * 【代码归属】成员三从零实现（challenge，不是框架提供）
 * 【判定依据】按 lab2.docx 的任务划分与代码内容判断，不依赖可能被清除的 lab2 / YOUR CODE 标记。
 * ============================================================================
 * 【答辩可以直接这样念】
 *
 *   “Buddy 是我们从零实现的，框架没有给任何代码。核心是把内存看成
 *    2 的幂大小、并且按自身大小对齐的块。申请 n 页时取
 *    k = ceil(log2(n))，找到 >= k 阶的空闲块后不断二分，
 *    上半块还回对应阶，低半块留给调用者。释放时用
 *    buddy = pfn XOR (1 << k) 找伙伴，只有伙伴同阶、空闲、
 *    仍是块首时才合并。
 *    最关键的接口设计是：分配时把真实阶数记在块首 property 里。
 *    SLUB 申请 3 页实际拿到 4 页块，释放时 free_pages(base,3)
 *    读回 order 2，完整归还 4 页，不会丢页，也不会误拆别人的块。”

 * ============================================================================
 * 【本组改动】本文件全部由成员三实现。
 * ============================================================================
 */
/*
 * ============================================================================
 * kern/mm/buddy_pmm.c —— Buddy System 物理页分配器（成员三，答辩重点）
 * ============================================================================
 * 【一句话原理】
 *   把内存看成大小都是 2^k 页、并且按自身大小对齐的块。
 *   申请 n 页时取 k = ceil(log2(n))，找一个 >= k 阶的空闲块，
 *   不断二分，把用不上的上半块还回去，留下低半块给调用者。
 *   释放时如果伙伴也空闲且同阶，就合并成高一阶的块，直到不能再并。
 *
 * 【答辩要点一：伙伴怎么算】
 *   对第 k 阶（2^k 页）的块，它的伙伴页号是：
 *
 *       buddy_pfn = pfn XOR (1 << k)
 *
 *   为什么能用异或？因为第 k 阶块一定按 2^k 对齐，
 *   页号的第 k 位就是"我是左半还是右半"：
 *     第 k 位为 0 → 伙伴在 +2^k；
 *     第 k 位为 1 → 伙伴在 -2^k。
 *   异或正好翻转这一位，两种方向都覆盖。
 *
 * 【答辩要点二：为什么必须对齐】
 *   如果块起址不是 2^k 对齐，异或算出来的地址就不在同一个父块里，
 *   可能去合并根本无关的页。所以 init_memmap 拆块、alloc 切块、
 *   free 合并时都保持"块首按自身大小对齐"这个不变量。
 *
 * 【答辩要点三：空闲结构】
 *   free_areas[0..NR_ORDERS-1] 是每阶一条链表；
 *   第 k 条链只放 2^k 页的块，块首 property = k，PG_property = 1。
 *   其余页不在链表上，也不带 PG_property。
 *
 * 【答辩要点四：最关键的接口设计——块首记录真实阶数】
 *   SLUB 的大对象保存的是"请求页数"：
 *
 *       npages = ceil((size + header) / PGSIZE);
 *       p = alloc_pages(npages);
 *       ...
 *       free_pages(p, npages);
 *
 *   如果请求 3 页，Buddy 只能给 4 页块（order 2）。
 *   若 free_pages(base, 3) 只归还 3 页，第 4 页就永久泄漏；
 *   更糟的是把它拆成 3 页块会破坏伙伴关系。
 *   本实现在 alloc 时把真实 order 写进块首 property，
 *   free 时读回这个 order，归还整个 2^order 块，
 *   同时校验 2^order >= n > 2^(order-1)，
 *   保证调用者传的 n 确实对应这个块。
 *
 * 【答辩要点五：nr_free 的记账】
 *   nr_free 是"空闲页数"，不是块数。
 *   插入一个块时按"传入块的阶"加一次；
 *   合并只是把两个已计数的空闲块并成一个，不能再加，
 *   否则伙伴的页会被重复统计。
 *
 * 【答辩要点六：失败必须无副作用】
 *   请求超过总空闲页、超过最大阶、或找不到合适阶时，
 *   直接返回 NULL，链表和计数都不许动。
 *
 * 本实现是单 hart 教学版本，没有并发锁、每 CPU 缓存、NUMA 或热插拔。
 * ============================================================================
 */
#include <buddy_pmm.h>
#include <defs.h>
#include <list.h>
#include <pmm.h>
#include <stdio.h>

//lab2 function3-1 2411738 秦东杰

/* MAX_ORDER = 16 表示阶号 0..15，最大块 2^15 页 = 128 MiB，
 * 正好覆盖 QEMU 默认的整块 RAM。 */
#define MAX_ORDER 16
#define NR_ORDERS MAX_ORDER

/* 每阶一条空闲链表；nr_free 是总空闲页数；buddy_ready 防止未初始化就分配。 */
static list_entry_t free_areas[NR_ORDERS];
static size_t nr_free;
static bool buddy_ready;

static void buddy_check(void);

/* 2^order，断言阶号合法。内部只在本文件用。 */
static size_t
pow2(size_t order) {
    assert(order < NR_ORDERS);
    return (size_t)1 << order;
}

/* 2^order，越界返回 0。
 * 分开写是为了避免在 order >= 64 时做未定义的移位。 */
static size_t
pow2_or_zero(size_t order) {
    if (order >= NR_ORDERS) {
        return 0;
    }
    return (size_t)1 << order;
}

/* *
 * buddy_order_of —— 求"能装下 n 页的最小阶"
 *
 *   返回最小的 k 使 2^k >= n；若这台机器的最大块也装不下，返回 NR_ORDERS。
 *   返回值饱和很重要：调用者会传 (size_t)-1 这类长度，
 *   如果这里写成 while (2^k < n) 而 2^k 越界变 0，就会死循环。
 * */
size_t
buddy_order_of(size_t n) {
    size_t order = 0;
    while (order < NR_ORDERS && pow2_or_zero(order) < n) {
        ++order;
    }
    return order;
}

/* *
 * buddy_of —— 求伙伴块首
 *
 *   用"描述符数组下标"做异或：
 *     index = page - pages       （相对下标）
 *     buddy_index = index ^ 2^k
 *
 *   为什么相对下标也能用？因为 nbase = 0x80000 = 2^19，
 *   而我们的阶最大 15（2^15），nbase 对所有可能的块大小都是对齐的，
 *   所以相对下标与绝对页号的异或结果一致。
 *
 *   越界（伙伴不在描述符数组内）返回 NULL，调用方据此拒绝合并。
 * */
static struct Page *
buddy_of(struct Page *page, size_t order) {
    size_t index = (size_t)(page - pages);
    size_t buddy_index = index ^ pow2(order);
    if (buddy_index >= npage - nbase) {
        return NULL;
    }
    return &pages[buddy_index];
}

/* *
 * buddy_mergeable —— 能不能合并
 *
 *   三个条件缺一不可：
 *     1. 伙伴地址存在（没越界）；
 *     2. 伙伴当前是空闲块头（PG_property = 1）；
 *     3. 伙伴的阶与我相同（property == order）。
 *
 *   只判断"物理相邻"是错的：相邻不等于互为伙伴，
 *   member3_buddy_test 的 T5 专门验证这一点。
 * */
static bool
buddy_mergeable(struct Page *page, size_t order) {
    struct Page *buddy = buddy_of(page, order);
    return buddy != NULL && PageProperty(buddy) && buddy->property == order;
}

/* *
 * buddy_insert —— 把空闲块插进对应阶，并尽可能向上合并
 *
 *   记账顺序很关键：
 *     先把"传入块"的页数加进 nr_free，
 *     再进入合并循环。
 *   因为伙伴块本来就在空闲链表里、已经被计入 nr_free，
 *   合并只是换一种组织方式，不能再加总页数。
 *
 *   合并时：
 *     - 从链表摘掉伙伴；
 *     - 清掉双方旧的 PG_property/property；
 *     - 取地址较小的那个作为新块首；
 *     - 阶号 +1，继续尝试与更高阶的伙伴合并。
 * */
static void
buddy_insert(struct Page *block, size_t order) {
    nr_free += pow2(order);
    while (order + 1 < NR_ORDERS && buddy_mergeable(block, order)) {
        struct Page *buddy = buddy_of(block, order);
        list_del(&buddy->page_link);
        ClearPageProperty(buddy);
        buddy->property = 0;
        ClearPageProperty(block);
        block->property = 0;
        if (buddy < block) {
            block = buddy;
        }
        ++order;
    }
    block->property = order;
    SetPageProperty(block);
    list_add(&free_areas[order], &block->page_link);
}

/* *
 * buddy_mark_allocated —— 交付前清理整块
 *
 *   把块内每一页的：
 *     - PG_property 清掉（不再空闲）；
 *     - PG_reserved 清掉（page_init 默认标了 Reserved）；
 *     - ref 归零、property 归零。
 *   最后只在块首写回真实 order，供 free_pages 使用。
 *
 *   【答辩要点】为什么不在整块每页都写 order？
 *   只有块首需要被 free 识别；内部页写 order 会让
 *   "某个页是不是块首"变得难以判断。
 * */
static void
buddy_mark_allocated(struct Page *block, size_t order) {
    for (size_t i = 0; i < pow2(order); ++i) {
        struct Page *p = block + i;
        ClearPageProperty(p);
        ClearPageReserved(p);
        set_page_ref(p, 0);
        p->property = 0;
    }
    block->property = order; // 真实阶数，free_pages 靠它恢复块大小
}

/* 初始化：清零所有阶链表和计数。此时还没有登记任何可用内存。 */
static void
buddy_init(void) {
    for (size_t i = 0; i < NR_ORDERS; ++i) {
        list_init(&free_areas[i]);
    }
    nr_free = 0;
    buddy_ready = 1;
}

/* 测试辅助：返回第 order 阶空闲链表上的块数。
 * 只给 member3_buddy_test 做"形状"比较用，不参与分配逻辑。 */
size_t
buddy_free_blocks(size_t order) {
    if (order >= NR_ORDERS) {
        return 0;
    }
    size_t count = 0;
    list_entry_t *le = &free_areas[order];
    while ((le = list_next(le)) != &free_areas[order]) {
        ++count;
    }
    return count;
}

/* *
 * buddy_init_memmap —— 登记一段可用物理页
 *
 *   page_init 交来的区间起点是任意的（本机是 0x8038c000），
 *   不一定按大阶对齐。所以不能"整段当成一个 2^k 块"，
 *   必须从当前地址出发，取"同时满足以下两条的最大阶"：
 *
 *     1. 2^order <= 剩余页数；
 *     2. 当前页号按 2^order 对齐。
 *
 *   然后登记这块、指针前移、剩余页数减少，直到处理完。
 *   这样拆出来的块天然满足伙伴算法要求的对齐。
 * */
static void
buddy_init_memmap(struct Page *base, size_t n) {
    assert(n > 0);
    assert(buddy_ready);
    struct Page *cur = base;
    size_t remaining = n;
    while (remaining > 0) {
        size_t pfn = page2ppn(cur);
        size_t order = 0;
        /* 能扩到多大？既要装得下，又要按 2^(order+1) 对齐。 */
        while (order + 1 < NR_ORDERS && pow2(order + 1) <= remaining &&
               (pfn & (pow2(order + 1) - 1)) == 0) {
            ++order;
        }
        /* 该块内所有页初始都是 Reserved，逐页清理成"可分配"。 */
        for (size_t i = 0; i < pow2(order); ++i) {
            struct Page *p = cur + i;
            assert(PageReserved(p));
            p->flags = 0;
            p->property = 0;
            set_page_ref(p, 0);
            list_init(&p->page_link);
        }
        buddy_insert(cur, order);
        cur += pow2(order);
        remaining -= pow2(order);
    }
}

/* *
 * buddy_alloc_pages —— 分配 n 个连续页
 *
 *   1. need = ceil(log2(n))，这是最小可用阶；
 *   2. 从 need 往上找，第一条非空链表就是可用的最小块；
 *   3. 取出块首，按阶二分：
 *        第 k 阶块拆成两个第 k-1 阶块，
 *        低半块继续拆，高半块挂回 free_areas[k-1]；
 *      直到降到 need 阶；
 *   4. 在块首记录 need，返回。
 *
 *   注意 nr_free 的账：
 *     取出整块时先减 2^找到的阶，
 *     每放回一个上半块再加回它的大小，
 *     最终净减少正好是 2^need。
 *
 *   失败：n 超过总空闲页、或没有任何阶有块，返回 NULL。
 *   此时没有修改任何结构（检查都发生在动手之前）。
 * */
static struct Page *
buddy_alloc_pages(size_t n) {
    assert(n > 0 && buddy_ready);
    size_t need = buddy_order_of(n);
    if (need >= NR_ORDERS || n > nr_free) {
        return NULL; // 总页数够也可能因为碎片找不到块
    }
    size_t order = need;
    while (order < NR_ORDERS && list_empty(&free_areas[order])) {
        ++order;
    }
    if (order == NR_ORDERS) {
        return NULL;
    }
    struct Page *block = le2page(list_next(&free_areas[order]), page_link);
    list_del(&block->page_link);
    ClearPageProperty(block);
    block->property = 0;
    nr_free -= pow2(order);

    /* 把上半块逐级还回去，低半块留给调用者。 */
    while (order > need) {
        --order;
        struct Page *upper = block + pow2(order);
        upper->property = order;
        SetPageProperty(upper);
        list_add(&free_areas[order], &upper->page_link);
        nr_free += pow2(order);
    }
    buddy_mark_allocated(block, need);
    return block;
}

/* *
 * buddy_free_pages —— 归还一个块
 *
 *   【答辩要点一：order 从哪来】
 *   从块首 property 读回来，而不是只看 n。
 *   这是解决"SLUB 请求 3 页、实际拿 4 页"问题的关键。
 *
 *   【答辩要点二：为什么要断言 n 的范围】
 *   2^order >= n > 2^(order-1) 说明 n 正好落在这一阶。
 *   如果调用者传了个不匹配的 n，说明它和分配器对块大小的理解不一致，
 *   继续执行会破坏伙伴结构，所以直接断言失败。
 *
 *   【答辩要点三：为什么不能只释放半块】
 *   块首记录的是一整块的阶；拆半释放会留下无法追踪的碎片。
 *   需要小页就重新 alloc_pages，而不是对已分配块做部分释放。
 *
 *   通过校验后，把整块重置成空闲状态，交给 buddy_insert 合并。 */
static void
buddy_free_pages(struct Page *base, size_t n) {
    assert(n > 0 && buddy_ready);
    assert(base != NULL);
    size_t order = base->property; // alloc_pages 记录的真实阶数
    assert(order < NR_ORDERS);
    assert(!PageReserved(base));
    assert(!PageProperty(base)); // 已经是空闲块头 → 重复释放
    /* 调用者请求的页数必须与这个块匹配。 */
    assert(pow2(order) >= n);
    assert(order == 0 || n > pow2(order - 1));

    for (size_t i = 0; i < pow2(order); ++i) {
        struct Page *p = base + i;
        assert(!PageReserved(p));
        assert(!PageProperty(p));
        p->flags = 0;
        p->property = 0;
        set_page_ref(p, 0);
    }
    buddy_insert(base, order);
}

/* 空闲页总数。 */
static size_t
buddy_nr_free_pages(void) {
    return nr_free;
}

/* *
 * buddy_count_free —— 从各阶链表重新累加空闲页数
 *
 *   用来交叉验证 nr_free：
 *     - 每个块都在正确阶的链表上；
 *     - 块首 property == 当前阶；
 *     - 块首按自身大小对齐。
 *   任何一条不满足，都说明不变量已经破了。
 * */
static size_t
buddy_count_free(void) {
    size_t total = 0;
    for (size_t order = 0; order < NR_ORDERS; ++order) {
        list_entry_t *le = &free_areas[order];
        while ((le = list_next(le)) != &free_areas[order]) {
            struct Page *p = le2page(le, page_link);
            assert(PageProperty(p));
            assert(p->property == order);
            assert((page2ppn(p) & (pow2(order) - 1)) == 0);
            total += pow2(order);
        }
    }
    return total;
}

/* 比较"当前各阶块数"与快照；不一致就打印阶号、块数和阶段。 */
static void
buddy_check_orders(const size_t *expect, const char *stage) {
    for (size_t order = 0; order < NR_ORDERS; ++order) {
        size_t got = buddy_free_blocks(order);
        if (got != expect[order]) {
            panic("buddy: order %d block count %d != %d after %s\n",
                  (int)order, (int)got, (int)expect[order], stage);
        }
    }
}

/* *
 * buddy_check —— 启动时自检（pmm_manager->check 调用它）
 *
 *   覆盖：
 *     1. 空闲计数与链表一致；
 *     2. 分配块的起址按自身大小对齐、块首记录真实阶；
 *     3. 乱序释放后各阶块数恢复原状（不只看总页数）；
 *     4. 非 2 的幂请求能完整往返、不丢页；
 *     5. 超量请求返回 NULL 且不改动状态。
 * */
static void
buddy_check(void) {
    assert(buddy_ready);
    assert(buddy_count_free() == nr_free);
    assert(nr_free <= npage - nbase);

    size_t before[NR_ORDERS];
    for (size_t order = 0; order < NR_ORDERS; ++order) {
        before[order] = buddy_free_blocks(order);
    }

    /* 依次申请 1、2、4、…、256 页，检查对齐和阶记录。 */
    struct Page *blocks[9];
    for (size_t order = 0; order <= 8; ++order) {
        blocks[order] = buddy_alloc_pages(pow2(order));
        assert(blocks[order] != NULL);
        assert((page2ppn(blocks[order]) & (pow2(order) - 1)) == 0);
        assert(blocks[order]->property == order);
        assert(!PageProperty(blocks[order]));
    }

    /* 故意打乱顺序释放：合并逻辑不能依赖 LIFO。 */
    free_pages(blocks[5], pow2(5));
    free_pages(blocks[2], pow2(2));
    free_pages(blocks[8], pow2(8));
    free_pages(blocks[0], pow2(0));
    free_pages(blocks[6], pow2(6));
    free_pages(blocks[1], pow2(1));
    free_pages(blocks[7], pow2(7));
    free_pages(blocks[3], pow2(3));
    free_pages(blocks[4], pow2(4));
    buddy_check_orders(before, "scrambled free");
    assert(buddy_count_free() == nr_free);

    /* 3 页请求：实际拿 4 页块，必须能完整还回去。 */
    size_t saved = nr_free;
    struct Page *odd = alloc_pages(3);
    assert(odd != NULL);
    assert(odd->property == 2);
    free_pages(odd, 3);
    assert(nr_free == saved);
    buddy_check_orders(before, "odd size");

    /* 超量请求：失败且不能改变空闲结构。 */
    struct Page *none = alloc_pages(nr_free + 1);
    assert(none == NULL);
    buddy_check_orders(before, "oom");
    assert(buddy_count_free() == nr_free);

    cprintf("buddy_check() succeeded! (alignment, order, split, merge, oom)\n");
}

/* 把六个方法绑成 pmm_manager。PMM=buddy 时 pmm.c 会选中它。 */
const struct pmm_manager buddy_pmm_manager = {
    .name = "buddy_pmm_manager",
    .init = buddy_init,
    .init_memmap = buddy_init_memmap,
    .alloc_pages = buddy_alloc_pages,
    .free_pages = buddy_free_pages,
    .nr_free_pages = buddy_nr_free_pages,
    .check = buddy_check,
};
