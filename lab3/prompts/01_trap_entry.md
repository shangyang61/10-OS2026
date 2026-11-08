# 功能一：本次实现采用的工作提示词

以下是依据本次用户指令、课程约束和现有代码整理的实现输入，
不是虚构的历史多轮对话。AI 实现和复核过程由本次 Codex 会话执行。

```text
阅读 Lab3 的 trapentry.S、trap.h、riscv.h 和 Lab2 的启动/栈/页表配置。
严格依据已有 SAVE_ALL 的布局填 RESTORE_ALL 与 __alltraps 两个标记。
保持 SAVE_ALL 和 struct trapframe 不变，不能换用其他项目的布局。
将当前 sp 放进 a0 后调用 trap，返回后恢复 sstatus、sepc、所有可写 GPR。
sp 必须最后从槽 2 恢复；不要在后续访存前破坏 trapframe 基址。
不要修改 SPP 来强制返回 U 模式，当前实验从 S 模式触发并返回 S 模式。
保留 sret，并解释 CSR、scratch 寄存器和 sp 的恢复次序。
现代 binutils 不认识 sbadaddr 时，在汇编头文件中定义 stval 兼容别名，
不要为消除编译错误修改 SAVE_ALL。
运行原始评分和真实异常返回测试，用日志验证，不能直接写 PASS。
```
