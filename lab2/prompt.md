# Lab2 成员二提示词与复核输入

Best-Fit 与 SLUB 初稿沿用本工作区此前的 AI 实现，本轮将其接入成员一已完成的基线，按下面规格复核、补充验证并撰写报告。本文件整理可复现的实现和复核输入，不冒充历史对话的逐字记录。

## Best-Fit 实现与接入

```text
阅读成员一交付中的 pmm.c、pmm.h、memlayout.h、default_pmm.c、
best_fit_pmm.c 和 libs/list.h。在 lab2 function2-2 标记处完成 Best-Fit。
保留成员一 page_init 函数体与其署名，不用旧参考版本替换。
保持 basic_check 和 best_fit_check 完全不变，兼容 free_list/nr_free 约定。
实现 init、init_memmap、alloc_pages、free_pages、nr_free_pages 和 manager。
链表按地址排序；申请选最小充分块，同长取低地址，exact fit 可停止。
分割保留剩余块；回收先前后后合并并维护头指针、标志和总页数。
在 pmm.c 真正选用 best_fit_pmm_manager。
解释不变量、First-Fit 对比、复杂度和可能改进。
运行真实 RISC-V 内核、原始评分与额外边界检查，不改测试绕过失败。
本人学号必须来自用户确认，不能挪用成员一学号。
```

## 简化 SLUB 实现

```text
基于已有页接口实现简化 SLUB，提供 slub_init/kmalloc/kfree。
16 至 2048 共八个大小类，每个 slab 一页，元数据位于页内头部。
用空闲槽位内嵌指针组成 freelist，位图记录槽位占用。
只将未满页加入 partial 链表，full 释放后重新加入，empty 立即还页。
大对象申请包含头部的连续多页并记下页数，注意溢出、OOM、0、NULL。
数据地址必须由物理页通过内核直接映射得到，不能写 Page 描述符。
不使用宿主机 malloc，不递归依赖 kmalloc 分配元数据。
在物理页管理成功初始化后接入，并明确只有单 hart 和16字节对齐。
配套自检覆盖规格边界、跨 slab、数据保持与不重叠、重用、页数恢复。
自检不冒充成员三的独立测试；保留真实编译和运行记录。
```

## First-Fit 缺陷复核

```text
检查 default_init_memmap 和 default_free_pages 的尾部插入分支。
在临时副本复现原代码，先确认框架自检通过，再检查双向链接不变量。
用实际运行说明缺少 break 是否会再次访问并插入新尾节点。
只修复定位到的分支，不修改 basic_check/default_check。
原版预期失败和修复后成功使用不同文件保存，不覆盖证据。
```

## 独立 SLUB 测试的组内交接草案

此项需由成员三独立执行，下面没有声称该独立工作已经完成。

```text
依据接口规格设计黑盒测试，独立于实现者自检。
覆盖各规格两侧、同页多对象、跨页、full→partial、乱序回收、
数据区间不重叠、payload保持、全部释放后页数恢复、OOM和极大长度。
非法释放的预期panic用独立启动测试，不将崩溃误报为正常通过。
若页层为Buddy，先明确请求页数与真实阶数的释放契约。
给出覆盖说明、为何能发现缺陷，以及真实环境和原始日志。
```
