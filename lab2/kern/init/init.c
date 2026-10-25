/*
 * ============================================================================
 * 【代码归属】课程实验框架为主；成员二、成员三做了接入修改
 * 【判定依据】按 lab2.docx 的任务划分与代码内容判断，不依赖可能被清除的 lab2 / YOUR CODE 标记。
 * ============================================================================
 * 【答辩可以直接这样念】
 *
 *   “kern_init 是 C 语言入口，执行顺序是：清 .bss、解析设备树、
 *    初始化控制台并打印启动标志、打印内核符号、pmm_init、
 *    再做对象层初始化和测试。
 *    清 .bss 是必须的，因为 .bss 不占镜像空间，加载后是内存残留，
 *    C 语言要求未初始化全局变量为 0。pmm_init 必须放在 dtb_init 之后，
 *    因为 page_init 要用设备树提供的物理内存范围。”

 * ============================================================================
 * 【本组改动】成员二接入 member2_page_check、slub_init、slub_check；成员三增加 USE_MEMBER3_TESTS 开关与 Buddy 测试分支。
 * ============================================================================
 */
/*
 * ============================================================================
 * kern/init/init.c —— C 语言内核入口（Lab2 答辩必讲启动流程）
 * ============================================================================
 * 【执行流】
 *   OpenSBI 跳转到 0x80200000
 *     → entry.S 的 kern_entry：设栈、建启动页表、写 satp、跳高地址
 *       → 本文件 kern_init()：从这里开始是 C 世界
 *
 * 【kern_init 依次做什么】
 *   1. 清 .bss（未初始化全局变量必须为 0，不能依赖加载器）；
 *   2. dtb_init：解析设备树，拿到物理内存范围；
 *   3. cons_init + cputs：把控制台准备好，打印第一行标志；
 *   4. print_kerninfo：打印内核各段地址，方便 GDB 对照；
 *   5. pmm_init：选页管理器、建立页描述符、登记空闲页、跑自检；
 *   6. 页层补充检查；
 *   7. slub_init + slub_check：对象层初始化与自检；
 *   8. 按编译开关跑成员三的独立测试；
 *   9. 死循环：内核没有"退出"的概念，测试脚本靠发信号结束 QEMU。
 *
 * 【答辩要点一：为什么先清 BSS】
 *   .bss 不会出现在镜像文件里（只记录大小），加载后内容是内存残留。
 *   C 语言要求未初始化全局变量为 0，所以必须由内核自己 memset。
 *   edata 是 .data 结束、end 是 .bss 结束，两者之间正好是 .bss。
 *
 * 【答辩要点二：为什么 pmm_init 在 dtb_init 之后】
 *   page_init 需要从设备树读 memory 节点的 base/size。
 *   没有 DTB 信息就不知道物理内存范围。
 *
 * 【答辩要点三：为什么测试要分开关】
 *   默认构建（评分路径）只跑框架要求的检查，输出保持原样；
 *   MEMBER3_TESTS=1 时才跑成员三的 Buddy/SLUB 独立测试。
 *   Buddy 配置下不跑 member2_page_check，因为它含 Best-Fit 专属的
 *   精确地址断言，换算法后这些断言本来就不该成立。
 * ============================================================================
 */
#include <console.h>
#include <defs.h>
#include <pmm.h>
#include <stdio.h>
#include <string.h>
#include <dtb.h>
#include <slub.h>
#include <member3_check.h>

void member2_page_check(void);

/* noreturn 告诉编译器 kern_init 不会返回，避免它生成返回路径和告警。 */
int kern_init(void) __attribute__((noreturn));
void grade_backtrace(void);
void print_kerninfo(void);

/* *
 * print_kerninfo —— 打印内核符号与镜像大小
 *
 *   entry/etext/edata/end 都是链接脚本定义的符号，不是变量。
 *   答辩时可以对照 obj/kernel.sym 和 GDB 里的地址看是否一致。
 * */
void print_kerninfo(void) {
    extern char etext[], edata[], end[];
    cprintf("Special kernel symbols:\n");
    cprintf("  entry  0x%016lx (virtual)\n", (uintptr_t)kern_init);
    cprintf("  etext  0x%016lx (virtual)\n", etext);
    cprintf("  edata  0x%016lx (virtual)\n", edata);
    cprintf("  end    0x%016lx (virtual)\n", end);
    cprintf("Kernel executable memory footprint: %dKB\n",
            (end - (char*)kern_init + 1023) / 1024);
}

int kern_init(void) {
    extern char edata[], end[];

    /* 1. 清 .bss：edata ~ end 之间的内容必须归零。 */
    memset(edata, 0, end - edata);

    /* 2. 解析设备树，得到 memory 节点的 base/size。 */
    dtb_init();

    /* 3. 控制台初始化并打印启动标志。 */
    cons_init();  // init the console
    const char *message = "(THU.CST) os is loading ...\0";
    //cprintf("%s\n\n", message);
    cputs(message);

    /* 4. 打印内核符号，便于调试与报告取地址。 */
    print_kerninfo();

    // grade_backtrace();
    /* 5. 物理内存管理：选策略 → 建描述符 → 登记空闲页 → 自检。 */
    pmm_init();  // init physical memory management

    /* 6. Best-Fit 专属的补充检查。它断言了具体的物理地址与
     *    "一次申请全部空闲页"，这些在 Buddy 下不成立，所以跳过。 */
#if !defined(USE_BUDDY_PMM)
    member2_page_check();
#endif

    /* 7. 对象层：初始化八个规格，再跑实现者自检。 */
    slub_init();
    slub_check();

    /* 8. 成员三独立测试（只在 MEMBER3_TESTS=1 时编译进来）。 */
#if defined(USE_MEMBER3_TESTS)
#if defined(USE_BUDDY_PMM)
    member3_buddy_test();
#endif
    member3_slub_test();
#endif

    /* 9. 内核任务完成后空转；QEMU 由测试脚本终止。 */
    /* do nothing */
    while (1)
        ;
}
