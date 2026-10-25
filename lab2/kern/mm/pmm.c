/*
 * ============================================================================
 * kern/mm/pmm.c —— 物理内存管理的总入口（Lab2 答辩必讲）
 * ============================================================================
 * 【本文件在启动流程中的位置】
 *   kern_entry（entry.S）
 *     → kern_init（init.c）
 *       → pmm_init（本文件）
 *           → init_pmm_manager   选算法、初始化空闲结构
 *           → page_init          从 DTB 读内存范围，扣掉保留区，登记空闲页
 *           → check_alloc_page   跑该算法的自检
 *       → SLUB 初始化、测试
 *
 * 【答辩要点一：为什么分两层】
 *   pmm.c 只负责"策略无关的公共流程"：
 *     确定内存范围、算描述符位置、把可用页交出去。
 *   具体怎么选块、怎么合并，交给 pmm_manager 的函数指针。
 *   这样 First-Fit / Best-Fit / Buddy 三种实现可以并存，互不污染。
 *
 * 【答辩要点二：描述符数组放在哪里】
 *   内核镜像结束符号 end 之后，向上对齐到 4 KiB，就是 struct Page 数组。
 *   每个物理页一个描述符（40 字节）。这段内存必须标记为 Reserved，
 *   不能交给分配器，否则调用者写数据会把描述符覆盖掉。
 *
 * 【答辩要点三：空闲区从哪开始】
 *   描述符数组结束、再向上对齐 4 KiB。之前的所有页（OpenSBI、内核镜像、
 *   描述符本身）都保留。只有这段之后的完整页才能登记给分配器。
 * ============================================================================
 */
#include <default_pmm.h>
#include <best_fit_pmm.h>
#include <buddy_pmm.h>
#include <defs.h>
#include <error.h>
#include <memlayout.h>
#include <mmu.h>
#include <pmm.h>
#include <sbi.h>
#include <stdio.h>
#include <string.h>
#include <riscv.h>
#include <dtb.h>

/* ===== 全局状态 =====
 * pages        : 页描述符数组，每个物理页一项
 * npage        : 物理内存上界的绝对页号（不是页数！）
 * nbase        : DRAM 起始的绝对页号（0x80000000 >> 12 = 0x80000）
 * va_pa_offset : 直接映射偏移，虚拟 = 物理 + 这个值
 */
// virtual address of physical page array
struct Page *pages;
// amount of physical memory (in pages)
size_t npage = 0;
// the kernel image is mapped at VA=KERNBASE and PA=info.base
uint64_t va_pa_offset;
// memory starts at 0x80000000 in RISC-V
// DRAM_BASE defined in riscv.h as 0x80000000
const size_t nbase = DRAM_BASE / PGSIZE;

/* 启动页表：satp_virtual 是内核虚拟地址，写 satp 必须用物理地址。
 * 答辩要点：satp 里存的是根页表物理页号 + 模式位，不是虚拟地址。 */
// virtual address of boot-time page directory
uintptr_t *satp_virtual = NULL;
// physical address of boot-time page directory
uintptr_t satp_physical;

/* 当前选中的页分配策略。三种实现共享这个指针。 */
// physical memory management
const struct pmm_manager *pmm_manager;

static void check_alloc_page(void);

/* *
 * init_pmm_manager —— 选择并初始化页分配策略
 *
 *   编译期开关：
 *     PMM=buddy    → buddy_pmm_manager
 *     PMM=best_fit → best_fit_pmm_manager（默认，原始评分路径）
 *
 *   【答辩要点】为什么用编译期开关而不是运行时？
 *   课程评分脚本检查的就是 Best-Fit 路径，默认必须保持不变；
 *   Buddy 作为独立实验配置，用宏切换、各自构建，避免一个二进制里
 *   带两套全局状态互相干扰。
 *   代价是切换后必须 make clean，否则会复用另一套目标文件。
 * */
// init_pmm_manager - initialize a pmm_manager instance
static void init_pmm_manager(void) {
#if defined(USE_BUDDY_PMM)
    pmm_manager = &buddy_pmm_manager;
#elif defined(USE_BEST_FIT_PMM)
    pmm_manager = &best_fit_pmm_manager;
#else
    pmm_manager = &best_fit_pmm_manager;
#endif
    cprintf("memory management: %s\n", pmm_manager->name);
    pmm_manager->init(); // 初始化空闲链表/阶链表和计数器
}

/* 把可用物理页登记给当前策略。First/Best-Fit 会建一条按地址排序的块链；
 * Buddy 会把区间拆成按大小对齐的 2 的幂块。 */
// init_memmap - call pmm->init_memmap to build Page struct for free memory
static void init_memmap(struct Page *base, size_t n) {
    pmm_manager->init_memmap(base, n);
}

/* 三个公开接口都只是转调，保持上层与策略解耦。 */
// alloc_pages - call pmm->alloc_pages to allocate a continuous n*PAGESIZE
// memory
struct Page *alloc_pages(size_t n) {
    return pmm_manager->alloc_pages(n);
}

// free_pages - call pmm->free_pages to free a continuous n*PAGESIZE memory
void free_pages(struct Page *base, size_t n) {
    pmm_manager->free_pages(base, n);
}

// nr_free_pages - call pmm->nr_free_pages to get the size (nr*PAGESIZE)
// of current free memory
size_t nr_free_pages(void) {
    return pmm_manager->nr_free_pages();
}

/* *
 * ============================================================================
 * page_init —— 成员一实现（2411804）：物理内存发现与保留区处理
 * ============================================================================
 * 【答辩要点一：内存范围从哪来】
 *   不是猜的，也不是写死 128 MiB。OpenSBI 把设备树（DTB）地址放在 a1，
 *   entry.S 保存后由 dtb_init 解析，get_memory_base/size 返回 memory 节点
 *   的 reg。这样换 QEMU 内存大小也能正确工作。
 *
 * 【答辩要点二：为什么要 ROUNDDOWN / ROUNDUP】
 *   分配器只管理完整页：
 *     managed_end = ROUNDDOWN(mem_end, PGSIZE)   上界向下取整
 *     pages_pa    = ROUNDUP(kernel_end, PGSIZE)  描述符起点向上取整
 *     free_begin  = ROUNDUP(pages_end, PGSIZE)   空闲区起点向上取整
 *   不这么做会把半个页交出去，或者让描述符跨页重叠。
 *
 * 【答辩要点三：哪些区域必须保留】
 *   1. OpenSBI 和内核镜像（0x80000000 ~ end）；
 *   2. struct Page 描述符数组本身；
 *   3. 任何不属于 memory 节点、或超出 PHYSICAL_MEMORY_END 的部分。
 *   先把所有描述符标 Reserved，再把空闲区交给 init_memmap，
 *   这样"默认不可分配、只显式释放"，比反过来更安全。
 *
 * 【答辩要点四：为什么先 SetPageReserved 再登记】
 *   init_memmap 会清掉传入区间的 Reserved 并挂到空闲链表。
 *   其余页保持 Reserved，分配器就不可能误分配内核或描述符。
 *
 * 注意：本函数在 specs/protected_functions.json 的保护清单里，
 * 函数体必须保持成员一交付的原样，连内部注释都不能随意增删，
 * 否则 tests/check_preserved.py 的 SHA256 校验会失败。
 * ============================================================================
 */
//lab2 function1 2411804
static void page_init(void) {
    extern char end[];
    uint64_t mem_base = get_memory_base();
    uint64_t mem_size = get_memory_size();
    uint64_t mem_end;
    uintptr_t managed_end, pages_pa, free_begin;
    size_t page_count, descriptor_bytes;

    va_pa_offset = PHYSICAL_MEMORY_OFFSET;
    /* Reject missing DTB data and overflow before using the range. */
    if (mem_base == 0 || mem_size == 0 ||
        mem_size > ~(uint64_t)0 - mem_base) {
        panic("page_init: invalid physical memory range");
    }
    mem_end = mem_base + mem_size;
    if (mem_base > DRAM_BASE || mem_end <= KERNEL_BEGIN_PADDR) {
        panic("page_init: memory does not cover the kernel DRAM base");
    }
    cprintf("physical memory map:\n");
    cprintf("  memory: 0x%016lx, [0x%016lx, 0x%016lx].\n",
            mem_size, mem_base, mem_end - 1);

    managed_end = mem_end < PHYSICAL_MEMORY_END ?
                  mem_end : PHYSICAL_MEMORY_END;
    managed_end = ROUNDDOWN(managed_end, PGSIZE);
    npage = PPN(managed_end);  /* Absolute end PPN, not the count of DRAM pages. */
    if (npage <= nbase) {
        panic("page_init: no manageable physical pages");
    }
    page_count = npage - nbase;
    descriptor_bytes = page_count * sizeof(struct Page);
    pages_pa = PADDR(end);
    if (pages_pa < KERNEL_BEGIN_PADDR || pages_pa > managed_end ||
        managed_end - pages_pa < PGSIZE - 1) {
        panic("page_init: kernel leaves no room for page descriptors");
    }
    pages_pa = ROUNDUP(pages_pa, PGSIZE);
    if (descriptor_bytes > managed_end - pages_pa) {
        panic("page_init: page descriptors exceed physical memory");
    }
    pages = (struct Page *)(pages_pa + va_pa_offset);
    for (size_t i = 0; i < page_count; i++) {
        pages[i].flags = 0;
        SetPageReserved(&pages[i]);
        set_page_ref(&pages[i], 0);
        pages[i].property = 0;
        list_init(&pages[i].page_link);
    }

    free_begin = ROUNDUP(pages_pa + descriptor_bytes, PGSIZE);
    if (free_begin < managed_end) {
        init_memmap(pa2page(free_begin),
                    (managed_end - free_begin) / PGSIZE);
    }
}

/* pmm_init - initialize the physical memory management */
void pmm_init(void) {
    // We need to alloc/free the physical memory (granularity is 4KB or other size).
    // So a framework of physical memory manager (struct pmm_manager)is defined in pmm.h
    // First we should init a physical memory manager(pmm) based on the framework.
    // Then pmm can alloc/free the physical memory.
    // Now the first_fit/best_fit/worst_fit/buddy_system pmm are available.
    init_pmm_manager();

    // detect physical memory space, reserve already used memory,
    // then use pmm->init_memmap to create free page list
    page_init();

    // use pmm->check to verify the correctness of the alloc/free function in a pmm
    check_alloc_page();

    extern char boot_page_table_sv39[];
    satp_virtual = (pte_t*)boot_page_table_sv39;
    satp_physical = PADDR(satp_virtual);
    cprintf("satp virtual address: 0x%016lx\nsatp physical address: 0x%016lx\n", satp_virtual, satp_physical);
}

static void check_alloc_page(void) {
    pmm_manager->check();
    cprintf("check_alloc_page() succeeded!\n");
}
