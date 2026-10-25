/*
 * ============================================================================
 * kern/debug/assert.h —— 断言与内核错误宏（Lab2 答辩常问）
 * ============================================================================
 * 【assert 做了什么】
 *   条件为假时调用 panic，把"哪个文件、哪一行、哪个表达式"打印出来。
 *   它不会"返回失败"，而是直接停住内核。
 *
 * 【答辩要点一：为什么断言里用 do { } while (0)】
 *   保证宏在 if/else 里当一条语句使用不会出问题，例如：
 *       if (x) assert(y); else ...
 *   如果直接展开成 { if (...) ...; }，后面的分号会让 else 悬空。
 *
 * 【答辩要点二：#x 是什么】
 *   字符串化。assert(p != NULL) 失败时打印的正是 "p != NULL"，
 *   不用手写错误信息。
 *
 * 【答辩要点三：断言不是错误处理】
 *   断言失败代表"不变量已经破了、继续跑只会更糟"。
 *   可恢复的错误（比如内存不够）应该返回 NULL，而不是 assert。
 *
 * 【答辩要点四：static_assert】
 *   编译期检查，条件必须能在编译期求值。
 *   这里用 switch 技巧：若 x 为 0，case 0 和 case x 重复 → 编译报错。
 * ============================================================================
 */
#ifndef __KERN_DEBUG_ASSERT_H__
#define __KERN_DEBUG_ASSERT_H__

#include <defs.h>

void __warn(const char *file, int line, const char *fmt, ...);
void __noreturn __panic(const char *file, int line, const char *fmt, ...);

/* 只警告 */
#define warn(...)                                       \
    __warn(__FILE__, __LINE__, __VA_ARGS__)

/* 致命错误：打印后死循环 */
#define panic(...)                                      \
    __panic(__FILE__, __LINE__, __VA_ARGS__)

/* 断言：条件不成立就 panic，并打印表达式本身 */
#define assert(x)                                       \
    do {                                                \
        if (!(x)) {                                     \
            panic("assertion failed: %s", #x);          \
        }                                               \
    } while (0)

// static_assert(x) will generate a compile-time error if 'x' is false.
#define static_assert(x)                                \
    switch (x) { case 0: case (x): ; }

#endif /* !__KERN_DEBUG_ASSERT_H__ */
