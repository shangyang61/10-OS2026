/*
 * ============================================================================
 * 【代码归属】成员三实现
 * 【判定依据】按 lab2.docx 的任务划分与代码内容判断，不依赖可能被清除的 lab2 / YOUR CODE 标记。
 * ============================================================================
 * 【答辩可以直接这样念】
 *
 *   “这里声明成员三的两个测试入口。Buddy 测试只在 PMM=buddy 时有意义；
 *    SLUB 测试对 Best-Fit 和 Buddy 都成立，所以两种配置都会跑。”

 * ============================================================================
 * 【本组改动】本文件由成员三实现。
 * ============================================================================
 */
/*
 * ============================================================================
 * member3_check.h —— 成员三独立测试入口
 * ============================================================================
 * Buddy 测试只在 PMM=buddy 时有意义；SLUB 测试对两种页管理器都成立。
 * ============================================================================
 */
#ifndef __KERN_MM_MEMBER3_CHECK_H__
#define __KERN_MM_MEMBER3_CHECK_H__

/* Independent tests written by member three.
 *
 * These are deliberately kept apart from the implementers' self checks
 * (member2_page_check, slub_check). They use only the public interfaces, so
 * they can run against whichever page manager is compiled in. */

/* Page-level tests for the Buddy System. Only meaningful with PMM=buddy. */
void member3_buddy_test(void);

/* Object-level tests for SLUB. Runs under any page manager. */
void member3_slub_test(void);

#endif
