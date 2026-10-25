# Lab2 物理内存和页表

当前目录已整合三位成员的初始化、Best-Fit、Buddy、简化 SLUB 与独立测试。完整实验报告见 [实验报告.md](实验报告.md)，已插入本人于 2026-10-25 在 VMware 中取得的 8 张实际截图；来源见 [images/README.md](images/README.md)，复现命令与调试过程见实验报告第八、十节。

## 当前状态

- 成员一 page_init、原报告及 VMware 原始日志已保留。
- 默认管理器为 Best-Fit，原有两个测试未改；成员二补充页测试与 SLUB 实现自检通过。
- 成员三补齐 Buddy System（`PMM=buddy`）与独立的 Buddy/SLUB 测试。
- 成员三在本机 VMware + 课程 QEMU 4.1.1 实测：Buddy 6 组、SLUB 8 组全部通过。
- 默认 Best-Fit 路径通过回归：原始评分 20/20，初始化边界 10/10，First-Fit 通过。
- 邹俊轩在本人 VMware 中再次完成上述回归及两种页管理器集成验证，并记录初始化断点、Bare 翻译与软件 TLB 填充现场；本次截图与成员一的 Sv39/TLB 原始日志分别注明来源。
- 成员二为邹俊轩（2412592），成员三为秦东杰（2411738）；代码标记与报告署名已补齐。

## 课程环境 QEMU 4.1.1

进入本目录并设置课程工具链 PATH：

```sh
export PATH=/opt/riscv/bin:$PATH
make clean
make PMM=best_fit
make qemu PMM=best_fit QEMU=/实际课程QEMU路径/qemu-system-riscv64
python3 tests/run_checks.py --profile course --qemu /实际课程QEMU路径/qemu-system-riscv64 --grade --first-fit --log-prefix member2_vm
```

成员三已在 VMware 的课程 QEMU 4.1.1 环境完成集成，路径为 `/opt/ucore-manual/qemu-4.1.1/bin/qemu-system-riscv64`，证据见 logs/member3_vm_* 与 member3_bestfit_128M_*。成员一和成员二的历史环境与日志另行保留，具体区别见报告。其他机器复跑时先核对自己的工具路径。

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
make qemu PMM=buddy MEMBER3_TESTS=1 QEMU=/实际课程QEMU路径/qemu-system-riscv64
```

或一次跑完两种配置并留下日志：

```sh
python3 tests/run_member3_checks.py \
  --qemu /opt/ucore-manual/qemu-4.1.1/bin/qemu-system-riscv64 \
  --log-prefix member3_vm
```

契约见 [成员三规格](specs/member3.md)，实现与测试说明见
[成员三工作说明](reports/成员三工作说明.md)，prompt 见 `prompts/member3/`。

`member2_page_check` 含 Best-Fit 的地址选择假设，Buddy 下不运行；`slub_check`
已为 Buddy 增加逐页耗尽分支，两种配置都运行。MEMBER3_TESTS=1 进一步运行
独立测试，当前完整结果以实验报告和成员三日志为准。

## 复核命令

```sh
python3 tests/check_preserved.py
python3 tests/member1/test_page_init.py
python3 tests/reproduce_first_fit_bug.py --profile portable
python3 tests/check_boot_profiles.py
```

最后一项会在临时目录分别构建两种配置并检查镜像根 PTE。缺陷复现命令以“观察到旧版本的指定断言”为成功，不修改交付代码。

logs 中保留原始输出和机器环境，测试不等价于对所有输入与多核并发的证明。成员二自检不代替成员三独立验收。
