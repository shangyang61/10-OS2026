# 功能二：本次实现采用的工作提示词

本文件整理实际采用的约束和复核输入，不声称是历史聊天逐字稿。

```text
阅读课程练习页、trap.c、clock.c、sbi.h 和官方评分脚本。
在 lab3 function2 区域实现 interrupt_handler 和 exception_handler，
保留其他函数原样，并用本组学号 2411804、2411738、2412592 替换 YOUR CODE。
中断原因去掉最高标志位，再依据框架 IRQ 宏分类；未知中断打印原因和现场。
S 时钟中断必须每次重新预约下一次事件、ticks++，每100次调用 print_ticks。
输出次数达到 SHUTDOWN_AFTER 时调用 sbi_shutdown。原包若漏定义该宏，
显式记录差异并默认10次，可由编译宏覆盖，不能跳过关机要求。
非法指令和断点分别打印 Illegal instruction at 和 Breakpoint at，
依据指令低两位区分16/32位并更新 tf->epc，其他异常打印 trapframe。
禁止把所有 epc 都加4，也禁止为匹配评分输出修改原始测试。
补充测试应在隔离副本执行；保存真实 bug、修复依据和运行结果。
```
