# Lab2 迁移与验证提示词

这里整理本次用户请求、实际工程约束与复核输入，不伪造其他成员的历史对话。

## 迁移输入

```text
[PROMPT]
读取当前仓库 Lab2，下载课程发布的 Lab3 原包，在 labcodes/lab3 完成实验。

[RELY]
Lab2 已有 page_init、Best-Fit、First-Fit 修复、Buddy、SLUB 及独立测试。
Lab3 原包提供同步包装、Trap 框架、SBI 关机和官方注入/评分脚本。

[GUARANTEE]
不覆盖 Lab2 原目录，默认使用 Best-Fit，保留 Buddy 的可选构建。
新工程可以独立构建运行，原始作者标记与受保护函数保持。

[SPECIFICATION]
迁入原函数体，保留 Lab3 页接口的中断保护，不能直接丢掉临界区包装。
保留课程 loader 默认启动；PORTABLE_BOOT=1 沿用 Lab2 的现代启动兼容。
Lab3 新标记使用本组学号 2411804、2411738、2412592。
报告列出石昊洋、秦东杰、邹俊轩，并参考 GitHub 上前两次报告的写法。
```

## 验证输入

```text
[PROMPT]
对完成的 Lab3 运行官方评分与额外真实 RISC-V 测试，保存原始证据。

[RELY]
使用现有课程 QEMU 4.1.1、现代 QEMU 10.2.1、交叉 GCC、gdb-multiarch。

[GUARANTEE]
不能改官方测试通过评分；不能用绘图伪造截图或凭预期填写结果。
保留失败与成功日志，报告说明实际问题与修复。

[SPECIFICATION]
额外测试在临时副本注入，覆盖16/32位断点和非法指令、GPR/SP、
嵌套中断状态与页接口、1000ticks正常关机。
覆盖64/128/256MiB、Buddy与现代QEMU；分别保存配置与返回码。
用GDB观察真实frame大小、S时钟原因、EPC/SP和SIE返回状态。
截图使用真实终端窗口；查看旧日志的截图必须说明证据来源。
报告明确测试范围，不编造小组成员分别操作的历史或课程平台提交。
```
