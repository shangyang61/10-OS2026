/*
 * ============================================================================
 * default_pmm.h —— First-Fit 管理器声明
 * ============================================================================
 * 导出 default_pmm_manager，供 pmm.c 或隔离回归测试选择。
 * ============================================================================
 */
#ifndef __KERN_MM_DEFAULT_PMM_H__
#define  __KERN_MM_DEFAULT_PMM_H__

#include <pmm.h>

extern const struct pmm_manager default_pmm_manager;

#endif /* ! __KERN_MM_DEFAULT_PMM_H__ */

