# Prompt：从零实现并接入 Buddy System

## 用途

本条用于让实现者从零写出 Buddy 页分配器并接入 `pmm_manager`。它规定
接口与不变量，不规定具体代码结构，也不包含现成实现。

## 提示词

```text
你要为这个 RISC-V ucore 实验实现一个 Buddy System 物理页分配器，并接入
kern/mm/pmm.h 里的 struct pmm_manager。先读 pmm.c、pmm.h、memlayout.h、
libs/list.h 和成员二写好的 best_fit_pmm.c。

必须满足的接口与不变量：

1. 提供 buddy_pmm_manager，实现 init、init_memmap、alloc_pages、
   free_pages、nr_free_pages、check 六个方法。
2. 空闲块按 order 分链表，第 k 条链只放 2^k 页的块，最多 MAX_ORDER 阶。
3. 只有块首页 set PG_property，property 记录该块的 order。非块首页清零。
4. 块首物理页号必须按 2^k 对齐，否则 pfn XOR (1<<k) 不能定位伙伴。
5. init_memmap 收到的是一段任意对齐、任意长度的连续空闲区，必须把它拆
   成若干按自身大小对齐的 2 的幂块，不能假设起点是大阶对齐的。
6. alloc_pages(n) 取 k = ceil(log2(n))，从最小可用阶找块，逐级二分，
   把上半块放回对应阶链表，返回低半块。返回块必须对齐且不和别人重叠。
7. free_pages(base,n) 必须能和合法的同阶空闲伙伴逐级合并；只有同阶、
   仍是空闲块首的伙伴才可合并，不能按"物理相邻"就合并。
8. 关键：alloc_pages(n) 在块首记录真实 order。free_pages(base,n) 用它
   恢复真实块大小，而不是只按调用者传的 n 去猜。原因是上层 SLUB 的大对象
   保存的是请求页数（例如 3 页），而实际拿到的是 4 页块；释放时若只还 3
   页，会永久丢页，或错误拆开仍在使用的块。同时校验
   2^order >= n > 2^(order-1)，请求与块不符就断言失败。
9. 失败路径（超出总空闲页、超出最大阶、无合适阶）必须返回 NULL，且不
   改变任何链表和计数。
10. nr_free_pages() 必须等于所有阶链表块数乘 2^k 之和。
11. check() 要覆盖：各阶对齐、order 记录、拆分后合并、相邻非伙伴不合并、
    逐页耗尽、乱序归还后形状恢复、超量请求失败且无副作用。

约束：
- 不修改成员一的 page_init，不修改 best_fit_pmm.c 与 default_pmm.c 里的
  框架测试函数（见 specs/protected_functions.json）。
- 单 hart 教学实现，不要求并发保护，但要在文档中写明这一点。
- 启动栈只有 2 页（8 KiB）：测试里的指针数组必须放静态区，不能放大数组
  到栈上，不能递归。
- 不用 alloc_pages(全部空闲页) 来测试 Buddy：它的空闲区跨多个阶，这种
  请求可能合法失败。改为逐页持有来耗尽。
- 用真实编译和 QEMU 运行验证，不修改测试绕过失败。

完成后给我：实现文件、接口说明、不变量、与 Best-Fit 的差异、
以及真实运行日志。
```
