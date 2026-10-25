/*
 * ============================================================================
 * kern/mm/pmm.h —— 物理内存管理器的公共接口（Lab2 答辩必讲）
 * ============================================================================
 * 【这个文件解决什么问题】
 *   上层（SLUB、测试、将来的进程管理）不应该关心底层到底用
 *   First-Fit、Best-Fit 还是 Buddy。它只需要"给我 n 个连续页""把这块还回去"。
 *   于是框架抽象出 struct pmm_manager，用函数指针把六件事固定下来：
 *
 *     init          初始化自己的数据结构
 *     init_memmap   把一段可用物理页登记进分配器
 *     alloc_pages   分配 n 个连续页，返回块首页描述符
 *     free_pages    归还一块
 *     nr_free_pages 还有多少空闲页
 *     check         自检
 *
 * 【答辩要点一：这是什么设计模式】
 *   策略模式（strategy）/ 面向对象里的多态。C 没有虚函数，就用函数指针表。
 *   pmm.c 只认 pmm_manager 指针，换算法只需换一个全局指针，
 *   调用方一行都不用改。
 *
 * 【答辩要点二：alloc/free 的契约】
 *   - n 必须 > 0；
 *   - 返回的是"块首"的 struct Page*；
 *   - free 必须传回同一个块首，且请求页数要和分配时一致；
 *   - 分配失败返回 NULL，不能破坏已有空闲结构。
 *
 *   三种分配器对">= n"的处理不同：
 *     First/Best-Fit 精确切出 n 页；
 *     Buddy 向上取整到 2 的幂，可能多给。
 *   所以 Buddy 必须在块首记录真实阶数，free 时才能完整归还。
 *
 * 【答辩要点三：npage 是绝对页号，不是页数】
 *   npage = PPN(物理内存上界)，nbase = PPN(DRAM 起始)。
 *   描述符数组有 npage - nbase 项，可用页数还要再扣掉内核和描述符本身。
 * ============================================================================
 */
#ifndef __KERN_MM_PMM_H__
#define __KERN_MM_PMM_H__

#include <assert.h>
#include <defs.h>
#include <memlayout.h>
#include <mmu.h>
#include <riscv.h>

/* pmm_manager 的六个方法就是三种页分配算法的共同接口。
 * 具体实现见 default_pmm.c / best_fit_pmm.c / buddy_pmm.c。 */
// pmm_manager is a physical memory management class. A special pmm manager -
// XXX_pmm_manager
// only needs to implement the methods in pmm_manager class, then
// XXX_pmm_manager can be used
// by ucore to manage the total physical memory space.
struct pmm_manager {
    const char *name;  // 管理器名字，启动日志会打印，测试也用它判断当前策略
    /* 初始化空闲链表/阶链表、计数器。此时还没有任何可用内存登记。 */
    void (*init)(void);
    /* 把 [base, base+n) 这段刚被 page_init 认定为可用的物理页登记成空闲。 */
    void (*init_memmap)(struct Page *base, size_t n);
    /* 申请 n 个连续页，返回块首描述符；失败返回 NULL。 */
    struct Page *(*alloc_pages)(size_t n);
    /* 归还从 alloc_pages 拿到的块；n 必须与申请时一致。 */
    void (*free_pages)(struct Page *base, size_t n);
    /* 当前空闲页数（以页为单位，不是块数）。 */
    size_t (*nr_free_pages)(void);
    /* 启动时跑一遍自检；各算法覆盖的内容不同。 */
    void (*check)(void);
};

/* 全局当前策略。pmm_init 时按编译开关指向 Best-Fit 或 Buddy。 */
extern const struct pmm_manager *pmm_manager;

void pmm_init(void);

/* 对上层暴露的三个薄封装，内部转调 pmm_manager。 */
struct Page *alloc_pages(size_t n);
void free_pages(struct Page *base, size_t n);
size_t nr_free_pages(void); // number of free pages

/* 申请/释放一页的语法糖。 */
#define alloc_page() alloc_pages(1)
#define free_page(page) free_pages(page, 1)

/* *
 * PADDR —— 内核虚拟地址 → 物理地址
 *
 *   直接映射下 物理 = 虚拟 - va_pa_offset。
 *   先检查地址是否落在内核映射区（>= KERNBASE），
 *   防止把用户地址或非法指针悄悄算成一个"看起来合法"的物理地址。
 *
 *   【答辩要点】为什么不能只写 kva - offset？
 *   因为非法指针减完可能仍然落在物理内存范围内，后面写进去就是内存破坏。
 * */
#define PADDR(kva)                                                 \
    ({                                                             \
        uintptr_t __m_kva = (uintptr_t)(kva);                      \
        if (__m_kva < KERNBASE) {                                  \
            panic("PADDR called with invalid kva %08lx", __m_kva); \
        }                                                          \
        __m_kva - va_pa_offset;                                    \
    })

/* *
 * KADDR —— 物理地址 → 内核虚拟地址
 *
 *   框架里这段被注释掉了，因为实验用 page2pa + va_pa_offset 自行拼。
 *   若恢复，逻辑是：先检查页号 < npage，再加偏移。
 * */
/*
#define KADDR(pa)                                                \
    ({                                                           \
        uintptr_t __m_pa = (pa);                                 \
        size_t __m_ppn = PPN(__m_pa);                            \
        if (__m_ppn >= npage) {                                  \
            panic("KADDR called with invalid pa %08lx", __m_pa); \
        }                                                        \
        (void *)(__m_pa + va_pa_offset);                         \
    })
*/

/* ===== 全局描述符数组与页号边界 =====
 * pages  : struct Page 数组，每个物理页一项
 * npage  : 物理内存上界的绝对页号（不是页数）
 * nbase  : DRAM 起始的绝对页号（0x80000000 >> 12 = 0x80000）
 * va_pa_offset : 直接映射偏移
 *
 * 例：128 MiB 时 nbase=0x80000，npage=0x88000，
 *     描述符项数 = npage - nbase = 0x8000 = 32768。
 */
extern struct Page *pages;
extern size_t npage;
extern const size_t nbase;
extern uint64_t va_pa_offset;

/* struct Page 指针 → 绝对页号。page - pages 是数组下标（相对 nbase），
 * 加上 nbase 才是物理页号。这是 Lab2 最常考的一处换算。 */
static inline ppn_t page2ppn(struct Page *page) { return page - pages + nbase; }

/* 绝对页号 → 物理地址：页号左移 12 位。 */
static inline uintptr_t page2pa(struct Page *page) {
    return page2ppn(page) << PGSHIFT;
}

/* 引用计数：本实验的页分配器基本不用，但框架接口保留。
 * 分配时清零，释放时断言为 0，防止重复释放被掩盖。 */
static inline int page_ref(struct Page *page) { return page->ref; }

static inline void set_page_ref(struct Page *page, int val) { page->ref = val; }

static inline int page_ref_inc(struct Page *page) {
    page->ref += 1;
    return page->ref;
}

static inline int page_ref_dec(struct Page *page) {
    page->ref -= 1;
    return page->ref;
}

/* 物理地址 → struct Page*：先转绝对页号，再减 nbase 得到数组下标。
 * 越界直接 panic，避免访问到描述符数组之外。 */
static inline struct Page *pa2page(uintptr_t pa) {
    if (PPN(pa) >= npage) {
        panic("pa2page called with invalid pa");
    }
    return &pages[PPN(pa) - nbase];
}

/* 刷新 TLB。sfence.vm 是旧版 RISC-V 指令名；
 * 现代规范叫 sfence.vma。本实验的课程 QEMU 认前者。
 * 答辩要点：改了页表之后如果 TLB 里还有旧项，CPU 可能继续用旧映射。 */
static inline void flush_tlb() { asm volatile("sfence.vm"); }

/* 启动栈符号由 entry.S 定义；栈大小见 memlayout.h 的 KSTACKPAGE。 */
extern char bootstack[], bootstacktop[]; // defined in entry.S

#endif /* !__KERN_MM_PMM_H__ */
