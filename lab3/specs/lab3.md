# Lab3 实现规格

日期：2026-11-07。小组成员：石昊洋（2411804）、秦东杰（2411738）、邹俊轩（2412592）。
Lab3 新增代码使用小组共同标记，Lab2 原作者标记保留。
依据：课程 lab3 的全部 12 页指导书和 2026 年官方 lab3.zip。

## 前置依赖

保留 Lab2 的 page_init、Best-Fit、First-Fit 修复、Buddy 和 SLUB 实现。
默认选 Best-Fit，PMM=buddy 可切换。Lab3 的 alloc/free/nr_free_pages 使用
local_intr_save/local_intr_restore，恢复进入临界区之前的中断状态。
不修改 Lab2 原目录。保护 page_init 与四个原始页分配测试的函数体。

## 功能一：Trap 入口和恢复

- 只填 trapentry.S 的两个 lab3 function1 标记。
- SAVE_ALL 与 trap.h 原样保留；36 个 uintptr_t，RV64 共 288 B，保持 ABI 栈对齐。
- 以 sp 为 trapframe 基址，a0=sp，调用 trap(tf)，返回后进入 __trapret。
- RESTORE_ALL 恢复 sstatus/sepc 与所有可写通用寄存器，sp 最后从槽 2 恢复。
- 不恢复只用于诊断的 scause/stval，不硬编码清除 SPP，不修改中断返回 epc。
- 保留 sret；当前实验要支持 S→S 的 Trap 返回。

## 功能二：中断与异常

- 仅在 trap.c 的 lab3 function2 区域增加实现、必要宏和头文件。
- 中断原因去掉最高位后分类，软件/定时器/外部中断按现有 IRQ 宏输出。
- IRQ_S_TIMER 每次重设事件，ticks++，每 TICK_NUM=100 次调用原 print_ticks。
- 原包未定义 SHUTDOWN_AFTER，默认补为 10（输出次数），允许 -D 覆盖。
  第 10 次输出后 sbi_shutdown；不得为通过评分取消关机。
- 未知中断输出 Unknown interrupt: cause N 并 print_trapframe。
- 非法指令输出 Illegal instruction at 0x...，断点输出 Breakpoint at 0x...。
- 支持 RV64GC 的 16/32 位指令长度：从 tf->epc 读 uint16_t，低两位不是 11
  则 +2，否则 +4。仅针对实验可读取的内核指令；不宣称支持任意用户地址恢复。
- 其他异常只打印 trapframe。保留其他 trap.c 函数原样。

## 验收与证据

- 不修改官方 tools/inject.py、grade.sh、test_lab3*，运行 make grade，预期 100/100。
- 默认内核持续触发 1000 次时钟中断、恰好输出 10 行 100 ticks，并正常退出 QEMU。
- 补充真实 Trap 测试覆盖 16/32 位断点、16/32 位非法指令、寄存器和 sp 恢复、
  嵌套中断状态保存与页分配临界区；测试仅注入临时副本。
- 保存原始构建/运行/GDB 日志、环境、源码摘要和实际截图；报告明确结果范围。
