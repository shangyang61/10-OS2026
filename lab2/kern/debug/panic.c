/*
 * ============================================================================
 * kern/debug/panic.c —— 致命错误与警告输出
 * ============================================================================
 * 【答辩要点一：panic 和 warn 的区别】
 *   panic：不可恢复。打印文件名/行号/信息后进入死循环，
 *          不再返回调用者。测试脚本看到 "kernel panic" 就判定失败。
 *   warn ：只打印，不终止内核。
 *
 * 【答辩要点二：为什么死循环而不是关机】
 *   教学环境没有可靠的关机接口；死循环能让 QEMU 停住，
 *   串口输出保留，便于事后看现场。测试脚本负责终止进程。
 *
 * 【答辩要点三：is_panic 的作用】
 *   防止 panic 里再次 panic（例如打印过程中又触发断言），
 *   造成无限递归或输出风暴。
 * ============================================================================
 */
#include <defs.h>
#include <stdio.h>

static bool is_panic = 0;

/* *
 * __panic —— 不可恢复的致命错误
 *
 *   第一次进入时打印现场；后续再进来直接跳到死循环，避免递归。
 *   参数是可变参数，用法和 printf 一样：
 *       panic("bad pa 0x%lx", pa);
 * */
void
__panic(const char *file, int line, const char *fmt, ...) {
    if (is_panic) {
        goto panic_dead;
    }
    is_panic = 1;

    // print the 'message'
    va_list ap;
    va_start(ap, fmt);
    cprintf("kernel panic at %s:%d:\n    ", file, line);
    vcprintf(fmt, ap);
    cprintf("\n");
    va_end(ap);

panic_dead:
    while (1) {
        ;
    }
}

/* 只警告，不终止。用于"能继续，但值得记录"的异常。 */
/* __warn - like panic, but don't */
void
__warn(const char *file, int line, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    cprintf("kernel warning at %s:%d:\n    ", file, line);
    vcprintf(fmt, ap);
    cprintf("\n");
    va_end(ap);
}

/* 供外部查询是否已经 panic 过。 */
bool
is_kernel_panic(void) {
    return is_panic;
}
