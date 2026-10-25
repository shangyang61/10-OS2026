/*
 * ============================================================================
 * 【代码归属】课程实验框架提供；成员二增加了 PORTABLE_BOOT 的同址映射
 * 【判定依据】按 lab2.docx 的任务划分与代码内容判断，不依赖可能被清除的 lab2 / YOUR CODE 标记。
 * ============================================================================
 * 【答辩可以直接这样念】
 *
 *   “这是内核的第一条指令。OpenSBI 把 hartid 放在 a0、DTB 物理地址放在 a1，
 *    这里先保存它们，然后把启动页表的物理页号和 MODE=8 写进 satp，
 *    用 sfence.vma 刷新 TLB，再把栈指针设到高地址、跳到 kern_init。
 *    根表第 511 项把 0xFFFFFFFFC0000000 映射到 0x80000000，
 *    是一个 1 GiB 大页，所以 satp 一生效，高地址取指就落到物理内核。
 *    现代 QEMU 用 -kernel 启动时 PC 可能在低地址，所以额外加了根表第 2 项
 *    的同址映射，保证切换过程不断。”

 * ============================================================================
 * 【本组改动】成员二增加 BOOT_IDENTITY_MAP 条件编译块；其余为框架原样。
 * ============================================================================
 */
/*
 * ============================================================================
 * kern/init/entry.S —— 内核的第一条指令（Lab2 答辩必讲）
 * ============================================================================
 * 【它在哪里】
 *   加电 → MROM → OpenSBI（0x80000000）→ 跳 0x80200000 → 【本文件 kern_entry】
 *   链接脚本把 .text.kern_entry 放在最前面，所以这个标号就是 0x80200000。
 *
 * 【它做的四件事】
 *   1. 保存 OpenSBI 传来的 hartid 和 DTB 物理地址（a0/a1）；
 *   2. 把启动页表的物理页号写进 satp，开启 Sv39；
 *   3. sfence.vma 刷新 TLB；
 *   4. 设置栈指针，跳到高地址的 kern_init。
 *
 * 【答辩要点一：为什么用 .S 而不是 .c】
 *   这时 C 运行环境（栈）还不存在，只能直接操作寄存器和 CSR。
 *   后缀 .S 表示先过 C 预处理器，所以能 #include 头文件里的常量。
 *
 * 【答辩要点二：satp 里放什么】
 *     bit 63..60 = MODE（8 表示 Sv39）
 *     bit 43..0  = 根页表物理页号（PPN）
 *   所以代码先算出页表的物理地址，右移 12 位得到 PPN，
 *   再和 (8 << 60) 做或运算。
 *   注意：写进 satp 的必须是物理地址，不能是虚拟地址。
 *
 * 【答辩要点三：为什么能从物理地址跳到高地址】
 *   启动页表根表第 511 项把 0xFFFFFFFFC0000000 映射到 0x80000000，
 *   是一个 1 GiB 的大页叶子。satp 一生效，CPU 看到的
 *   0xFFFFFFFFC02000xx 就落到物理 0x802000xx。
 *
 * 【答辩要点四：PORTABLE_BOOT 的额外映射是干什么的】
 *   课程 QEMU 4.1.1 用 -device loader 把内核放到 0x80200000，
 *   当前 PC 本来就是高地址；现代 QEMU 用 -kernel 加载时 PC 可能在低地址，
 *   写 satp 的瞬间若没有低地址映射，取指会立刻失败。
 *   所以 BOOT_IDENTITY_MAP 在根表第 2 项加一个 0x80000000 的同址映射，
 *   让切换过程平滑。默认（课程配置）不启用。
 * ============================================================================
 */
#include <mmu.h>
#include <memlayout.h>

    .section .text,"ax",%progbits
    .globl kern_entry
kern_entry:
    # a0: hartid
    # a1: dtb physical address
    # 保存 hartid 和 DTB 地址，后面 dtb_init 会从这两个全局变量取
    la t0, boot_hartid
    sd a0, 0(t0)
    la t0, boot_dtb
    sd a1, 0(t0)

    # t0 := 三级页表的虚拟地址
    lui     t0, %hi(boot_page_table_sv39)
    # t1 := 0xffffffff40000000 即虚实映射偏移量
    li      t1, 0xffffffffc0000000 - 0x80000000
    # t0 减去虚实映射偏移量 0xffffffff40000000，变为三级页表的物理地址
    sub     t0, t0, t1
    # t0 >>= 12，变为三级页表的物理页号
    srli    t0, t0, 12

    # t1 := 8 << 60，设置 satp 的 MODE 字段为 Sv39
    li      t1, 8 << 60
    # 将刚才计算出的预设三级页表物理页号附加到 satp 中
    or      t0, t0, t1
    # 将算出的 t0(即新的MODE|页表基址物理页号) 覆盖到 satp 中
    csrw    satp, t0
    # 使用 sfence.vma 指令刷新 TLB，保证下面的取指使用新页表
    sfence.vma
    # 从此，我们给内核搭建出了一个完美的虚拟内存空间！
    #nop # 可能映射的位置有些bug。。插入一个nop

    # 我们在虚拟内存空间中：随意将 sp 设置为虚拟地址！
    # bootstacktop 在 .data 段，链接地址是高地址（KERNBASE 之后）
    lui sp, %hi(bootstacktop)

    # 我们在虚拟内存空间中：随意跳转到虚拟地址！
    # 用 lui + addi 拼出 kern_init 的完整 64 位地址后跳过去
    lui t0, %hi(kern_init)
    addi t0, t0, %lo(kern_init)
    jr t0

.section .data
    # .align 2^12 —— 栈必须按页对齐，方便以后加保护页
    .align PGSHIFT
    .global bootstack
bootstack:
    .space KSTACKSIZE        # 2 页 = 8 KiB，见 memlayout.h
    .global bootstacktop
bootstacktop:

.section .data
    # 页表必须放在一个 4 KiB 页里，所以按 12 位对齐
    .align PGSHIFT
    .global boot_page_table_sv39
# 分配 4KiB 内存给预设的三级页表
boot_page_table_sv39:
    # 根表第 511 项：VA 0xFFFFFFFFC0000000 → PA 0x80000000，1 GiB 大页
    # 0xffffffff_c0000000 map to 0x80000000 (1G)
    # Modern-QEMU profile only: preserve low-PC fetches across the satp switch.
#ifdef BOOT_IDENTITY_MAP
    .zero 8 * 2
    .quad (0x80000 << 10) | 0xcf   # 根表第 2 项：低地址 0x80000000 同址映射
    .zero 8 * (511 - 3)
#else
    # 前 511 个页表项均设置为 0 ，因此 V=0 ，意味着是空的(unmapped)
    .zero 8 * 511
#endif
    # 设置最后一个页表项，PPN=0x80000，标志位 VRWXAD 均为 1
    # 0xcf = V(1)|R(2)|W(4)|X(8)|A(0x40)|D(0x80)，是 1 GiB 叶子项
    .quad (0x80000 << 10) | 0xcf # VRWXAD

    .global boot_hartid
boot_hartid:
    .quad 0
    .global boot_dtb
boot_dtb:
    .quad 0
