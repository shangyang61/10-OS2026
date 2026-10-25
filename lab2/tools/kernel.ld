/*
 * ============================================================================
 * 【代码归属】课程实验框架提供（不是本组实现）
 * 【判定依据】按 lab2.docx 的任务划分与代码内容判断，不依赖可能被清除的 lab2 / YOUR CODE 标记。
 * ============================================================================
 * 【答辩可以直接这样念】
 *
 *   “链接脚本把内核链接到高地址 0xFFFFFFFFC0200000，因为 Lab2 是在
 *    Sv39 打开后的高地址空间运行。它把 .text.kern_entry 放在最前面，
 *    保证物理 0x80200000 处就是第一条指令。etext/edata/end 是位置符号，
 *    分别给代码段结束、数据段结束和镜像结束用；page_init 就是拿 end
 *    来算页描述符数组的起点。”

 * ============================================================================
 * 【本组改动】未改动。
 * ============================================================================
 */
/*
 * ============================================================================
 * tools/kernel.ld —— 内核链接脚本（Lab2 答辩必讲）
 * ============================================================================
 * 【链接脚本干什么】
 *   编译只把每个 .c/.S 变成 .o，段还没有最终地址。
 *   链接脚本负责：
 *     1. 把各输入文件的同名段拼成 .text/.rodata/.data/.bss；
 *     2. 指定这些段在内核虚拟地址空间中的位置；
 *     3. 定义 etext/edata/end 这些"位置符号"。
 *
 * 【答辩要点一：为什么 BASE_ADDRESS 是高地址】
 *   Lab2 的内核运行在 Sv39 打开之后的高地址空间：
 *     0xFFFFFFFFC0200000 = 物理 0x80200000 + 直接映射偏移
 *   entry.S 建好页表、写 satp 之后才跳到 kern_init，
 *   所以链接地址必须是高地址。镜像仍由 QEMU 加载到物理 0x80200000。
 *
 * 【答辩要点二：.text.kern_entry 必须排最前】
 *   链接脚本把 *(.text.kern_entry) 放在 .text 第一个，
 *   保证 kern_entry 就是内核镜像的第一条指令（物理 0x80200000）。
 *   否则 OpenSBI 跳过来会执行到别的函数。
 *
 * 【答辩要点三：etext/edata/end 是符号不是变量】
 *   PROVIDE 定义的是"当前位置"的地址，不占空间。
 *   init.c 用 edata~end 清 .bss；
 *   page_init 用 end 计算描述符数组的物理起点。
 *
 * 【答辩要点四：为什么要 ALIGN(0x1000)】
 *   数据段按页对齐，方便以后给段加页级权限；
 *   也让 .data 与 .text 不共享同一页的可执行权限。
 * ============================================================================
 */
/* Simple linker script for the ucore kernel.
   See the GNU ld 'info' manual' ("info ld") to learn the syntax. */

OUTPUT_ARCH(riscv)
ENTRY(kern_entry)

/* 内核虚拟起始地址：物理 0x80200000 + 0xFFFFFFFF40000000 */
BASE_ADDRESS = 0xFFFFFFFFC0200000;

SECTIONS
{
    /* Load the kernel at this address: "." means the current address */
    . = BASE_ADDRESS;

    /* 代码段：kern_entry 必须排第一，保证 0x80200000 处就是入口 */
    .text : {
        *(.text.kern_entry)
        *(.text .stub .text.* .gnu.linkonce.t.*)
    }

    PROVIDE(etext = .); /* 代码段结束，GDB/报告里用来算内核大小 */

    /* 只读数据：字符串常量、跳转表等 */
    .rodata : {
        *(.rodata .rodata.* .gnu.linkonce.r.*)
    }

    /* 数据段按页对齐 */
    . = ALIGN(0x1000);

    /* 已初始化数据 */
    .data : {
        *(.data)
        *(.data.*)
    }

    /* 小数据段：编译器对小全局变量可能放进 .sdata */
    .sdata : {
        *(.sdata)
        *(.sdata.*)
    }

    PROVIDE(edata = .); /* 已初始化数据结束；edata~end 就是 .bss */

    /* 未初始化数据：不占镜像空间，加载后由 init.c 清零 */
    .bss : {
        *(.bss)
        *(.bss.*)
        *(.sbss*)
    }

    PROVIDE(end = .); /* 内核镜像结束，页描述符数组从这里之后开始 */

    /* 丢弃不需要进镜像的段 */
    /DISCARD/ : {
        *(.eh_frame .note.GNU-stack)
    }
}
