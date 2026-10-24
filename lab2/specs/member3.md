# 成员三规格：Buddy System 与独立测试

作者：秦东杰，2411738。

## 范围

成员三负责 Buddy System 实现与接入、Buddy 的实现与测试两份 prompt、
独立 SLUB 测试 prompt 与测试代码、集成验收与报告汇总。

本次交付不修改成员一 `page_init()`，也不修改框架保护测试
`best_fit_check`、`default_check` 等五个函数（见
`specs/protected_functions.json`）。

## Buddy 接口契约

模块实现 `struct pmm_manager` 的六个方法，提供 `buddy_pmm_manager`。

| 方法 | 契约 |
|---|---|
| `init` | 初始化各阶空闲链表与总空闲页数 |
| `init_memmap(base,n)` | 把任意连续页区间拆成按自身大小对齐的 2 的幂块并登记 |
| `alloc_pages(n)` | 返回按 `2^k >= n` 对齐的块首描述符；n>0 |
| `free_pages(base,n)` | 归还 `alloc_pages(n)` 得到的**整个**块 |
| `nr_free_pages()` | 返回实际空闲页数 |
| `check()` | 自检不变量与分配/回收路径 |

### 页数与阶数的关系

这是本模块最关键的一条：

```text
alloc_pages(3)  -> 返回 4 页的块（order 2），块首 property 记录 2
free_pages(base,3) -> 由块首记录恢复 order 2，完整归还 4 页
```

只按调用者传入的 n 计算阶数并不够：调用者可能记错，或对象层保存的
是请求页数而不是真实阶数。本实现在**已分配块首的 `property` 字段记录
真实阶数**，释放时以该记录为准，同时用
`2^order >= n > 2^(order-1)` 校验调用者的请求与块大小一致，不一致就断言失败。

这条设计直接解决了交接文档指出的 SLUB 大对象风险：`kmalloc` 保存的是
`ceil((size+header)/PAGE)` 页数，`kfree` 用同一页数调用 `free_pages`，
两者算出同一个阶，不会丢页，也不会归还尚在使用的页。

`free_pages` **不接受**只释放大块的一部分。调用者必须传入
`alloc_pages` 返回的块首和相同的请求页数。

### 不变量

1. 空闲链表按阶组织，第 k 条链上的块大小恰为 2^k 页。
2. 块首页 `PG_property` 置位，`property == k`；非块首页两者都不置位。
3. 块首物理页号按 2^k 对齐，否则 `pfn XOR (1<<k)` 不能定位伙伴。
4. 已分配块首 `PG_property` 清零、`property == k`（记录真实阶数）。
5. 只有同阶、空闲、且为块首的伙伴才合并。
6. `nr_free_pages()` 等于各阶链表块数乘 2^k 之和。
7. 分配失败不改变任何链表或计数。

### 与 Best-Fit 的差异

| 项目 | Best-Fit | Buddy |
|---|---|---|
| 空闲结构 | 一条按地址排序的块链 | 每阶一条链 |
| 分割 | 切出恰好 n 页 | 按 2 的幂反复二分 |
| 释放参数 | 页数即块长 | 页数决定阶，块首另行记录真实阶 |
| 内部碎片 | 无 | 有，不超过请求的 2 倍 |
| `alloc_pages(全部空闲页)` | 通常成立 | 可能合法失败（空闲区跨多阶） |

最后一条是两套测试必须分开的原因：Best-Fit 的
`member2_page_check`、`slub_check` 依赖「一次申请全部空闲页能成功」和
「精确地址选择」，在 Buddy 下这些预期不成立，不能据此判定 Buddy 有错。

## 构建与运行入口

```sh
make clean
make PMM=buddy MEMBER3_TESTS=1
make qemu QEMU=/课程QEMU路径/qemu-system-riscv64

# 或一次性跑两种配置并留下日志
python3 tests/member3/run_member3_checks.py \
  --qemu /课程QEMU路径/qemu-system-riscv64
```

`PMM` 是编译期开关，切换必须 `make clean`，否则会复用另一套目标文件。
默认 `PMM=best_fit`，因此原始评分路径不受影响。

## 测试覆盖

### Buddy（`kern/mm/member3_buddy_test.c`）

| 编号 | 覆盖 | 能发现的缺陷 |
|---|---|---|
| T1 | 阶数取整契约 | 记录请求页数而非真实阶 |
| T2 | 逐页耗尽 + 逆序归还 | 丢页、错误合并、计数漂移 |
| T3 | 超量/溢出请求失败 | 失败路径破坏链表 |
| T4 | 最大块拆分后合并 | 拆分余块丢失、形状未恢复 |
| T5 | 相邻非伙伴不合并 | 只按物理相邻合并 |
| T6 | 各阶对齐 | 块起址未对齐导致伙伴计算错误 |

### SLUB（`kern/mm/member3_slub_test.c`）

| 编号 | 覆盖 | 能发现的缺陷 |
|---|---|---|
| T1 | 1~2048 各规格两侧边界 | 规格表错位、越界写 |
| T2 | 同页共享 + 跨页对象 | 每对象独占一页、跨页不足 |
| T3 | full→partial 复用 | 满页未重新入链 |
| T4 | 混合规格 240 个活对象 | 槽位重复发放、重叠 |
| T5 | 部分释放后存活对象 | 释放破坏邻居数据 |
| T6 | 2049~12288 B 多页对象 | 大对象页数丢失 |
| T7 | 0 / NULL / 溢出长度 | 截断或崩溃 |
| T8 | 真实页耗尽 | 页层为空时仍返回对象 |

T8 逐页持有全部空闲页，不依赖「一次申请全部空闲页」，因此对
Best-Fit 和 Buddy 都成立。

## 当前验证

本机 Ubuntu（VMware）、RISC-V GCC 14.2.0、课程 QEMU 4.1.1、128 MiB：

- `PMM=buddy MEMBER3_TESTS=1`：Buddy 6 项 + SLUB 8 项全部通过
- `PMM=best_fit MEMBER3_TESTS=1`：SLUB 8 项全部通过
- `PMM=best_fit` 原始 `make grade`：20/20（回归未受影响）

日志见 `logs/member3_vm_*`。
