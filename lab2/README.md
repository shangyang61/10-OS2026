# Lab2 物理内存和页表

当前目录汇总成员一交付，并接入成员二的 Best-Fit 与简化 SLUB。实验报告见 [实验报告.md](实验报告.md)。

交接入口：[交接给成员三.md](交接给成员三.md)；本机操作步骤：[VMware手动验证.md](VMware手动验证.md)。

## 当前状态

- 成员一 page_init、原报告及 VMware 原始日志已保留。
- 默认管理器为 Best-Fit，原有两个测试未改；成员二补充页测试与 SLUB 实现自检通过。
- 成员三补齐 Buddy System（`PMM=buddy`）与独立的 Buddy/SLUB 测试。
- 成员三在本机 VMware + 课程 QEMU 4.1.1 实测：Buddy 6 组、SLUB 8 组全部通过。
- 默认 Best-Fit 路径未回归：原始评分 20/20，初始化边界 10/10，First-Fit 通过。
- 成员二为邹俊轩（2412592），成员三为秦东杰（2411738）；代码标记与报告署名已补齐。

## 课程环境 QEMU 4.1.1

进入本目录并设置课程工具链 PATH：

```sh
export PATH=/opt/riscv/bin:$PATH
make
make qemu QEMU=/实际课程QEMU路径/qemu-system-riscv64
python3 tests/run_checks.py --profile course --qemu /实际课程QEMU路径/qemu-system-riscv64 --grade --first-fit --log-prefix member2_vm
```

成员一原环境的 QEMU 路径为 `/home/vmuser/src/qemu-4.1.1/riscv64-softmmu/qemu-system-riscv64`。本轮已定位用户配置的 VMware，用户补齐主机 VMnet8 临时地址后已能 ping 通客体，但 SSH 22 端口未监听；整合版本尚未在该 VMware 中实测。可按手动指南在 Ubuntu 终端直接复跑，当前 WSL 日志仍单独记录。

## 现代 QEMU 与 OpenSBI

```sh
make clean
make PORTABLE_BOOT=1
make qemu PORTABLE_BOOT=1
python3 tests/run_checks.py --profile portable --grade --first-fit --log-prefix member2
```

兼容配置使用 -kernel 与额外的启动同址映射。默认课程配置仍是原 loader 路径和单个高地址根项。切换配置后清理旧对象，避免 make 复用另一套参数生成的产物。

## 成员三：Buddy 与独立测试

`PMM` 是编译期开关，切换前必须 `make clean`：

```sh
make clean
make PMM=buddy MEMBER3_TESTS=1
make qemu QEMU=/实际课程QEMU路径/qemu-system-riscv64
```

或一次跑完两种配置并留下日志：

```sh
python3 tests/run_member3_checks.py \
  --qemu /opt/ucore-manual/qemu-4.1.1/bin/qemu-system-riscv64 \
  --log-prefix member3_vm
```

契约见 [成员三规格](specs/member3.md)，实现与测试说明见
[成员三工作说明](reports/成员三工作说明.md)，prompt 见 `prompts/member3/`。

`member2_page_check` 与 `slub_check` 含 Best-Fit 专属假设（见
`交接给成员三.md`），因此 Buddy 配置下单独运行成员三测试，不把它们当作
Buddy 的验收标准。

## 复核命令

```sh
python3 tests/check_preserved.py
python3 tests/member1/test_page_init.py
python3 tests/reproduce_first_fit_bug.py --profile portable
python3 tests/check_boot_profiles.py
```

最后一项会在临时目录分别构建两种配置并检查镜像根 PTE。缺陷复现命令以“观察到旧版本的指定断言”为成功，不修改交付代码。

logs 中保留原始输出和机器环境，测试不等价于对所有输入与多核并发的证明。成员二自检不代替成员三独立验收。
