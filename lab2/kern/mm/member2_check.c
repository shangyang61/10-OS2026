/*
 * ============================================================================
 * 【代码归属】成员二实现
 * 【判定依据】按 lab2.docx 的任务划分与代码内容判断，不依赖可能被清除的 lab2 / YOUR CODE 标记。
 * ============================================================================
 * 【答辩可以直接这样念】
 *
 *   “这是成员二额外的 Best-Fit 检查，覆盖框架没测到的点：
 *    最小充分块、同长选低地址、分割余块、乱序释放后的双侧合并，
 *    以及‘总页数够但没有连续 3 页时必须返回 NULL’的外部碎片场景。
 *    要注意这些断言依赖 Best-Fit 的具体地址和连续空闲区假设，
 *    所以 Buddy 配置下不运行它，不能拿它当 Buddy 的验收标准。”

 * ============================================================================
 * 【本组改动】本文件由成员二实现。
 * ============================================================================
 */
/*
 * ============================================================================
 * kern/mm/member2_check.c —— 成员二的 Best-Fit 补充页检查
 * ============================================================================
 * 【它和框架测试的区别】
 *   框架的 basic_check/best_fit_check 由课程提供，不能改；
 *   本文件是成员二额外写的检查，覆盖框架没测到的点。
 *
 * 【答辩要点：这些断言为什么只适用于 Best-Fit】
 *   1. 它断言了具体的物理地址（alloc_pages(4) == base + 9），
 *      依赖 Best-Fit "同长选低地址"的确定性选择；
 *   2. 它用 alloc_pages(before) 一次拿走整个空闲区，
 *      依赖 Best-Fit 把空闲区维护成一个连续块；
 *   3. 它按"申请 3 页就精确得到 3 页"来算地址。
 *   Buddy 会向上取整到 2 的幂，所以这些断言在 Buddy 下本来就不该成立，
 *   不能拿它当 Buddy 的验收标准。
 *
 * 【它验证了什么】
 *   - 最小充分块选择；
 *   - 同长时选低地址；
 *   - 分割余块；
 *   - 乱序释放后双侧合并；
 *   - 外部碎片：总页数够但没有连续 3 页时必须返回 NULL。
 * ============================================================================ */
#include <pmm.h>
#include <stdio.h>

// Additional implementation checks; framework checks remain unchanged.
void member2_page_check(void) {
    size_t before = nr_free_pages();
    struct Page *base = alloc_pages(32);
    assert(base != NULL);
    free_pages(base, 8);
    free_pages(base + 9, 5);
    free_pages(base + 15, 5);
    free_pages(base + 21, 3);
    // Choose smallest sufficient block; ties choose lower physical address.
    assert(alloc_pages(4) == base + 9);
    assert(alloc_pages(5) == base + 15);
    assert(alloc_pages(3) == base + 21);
    assert(alloc_pages(1) == base + 13);
    assert(alloc_pages(8) == base);
    assert(nr_free_pages() == before - 32);
    // All 32 pages are allocated now. Release out of order to join both sides.
    free_pages(base, 8);
    free_pages(base + 16, 16);
    free_pages(base + 8, 8);
    assert(nr_free_pages() == before);

    // Isolate the complete free region: enough total pages, no long enough block.
    base = alloc_pages(before);
    assert(base != NULL);
    assert(alloc_page() == NULL);
    free_pages(base, 2);
    free_pages(base + 3, 2);
    assert(nr_free_pages() == 4 && alloc_pages(3) == NULL);
    assert(alloc_pages(2) == base);
    assert(alloc_pages(2) == base + 3);
    free_pages(base, before);
    assert(nr_free_pages() == before);
    cprintf("member2_page_check() succeeded! (best choice, ties, merge, fragmentation)\n");
}
