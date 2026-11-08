# Lab3：中断与异常处理

小组：石昊洋（2411804）、秦东杰（2411738）、邹俊轩（2412592）。

完整说明见 [实验报告](实验报告.md)。官方评分 **100/100**，5 种集成配置通过，
默认内核在 1000 ticks 后调用 SBI 关机。实验原包、spec、prompt、失败与成功日志、
真实终端截图均在本目录保存。

## 构建和运行

```sh
cd lab3
export PATH=/opt/ucore-manual/qemu-4.1.1/bin:$PATH
make -j4
make qemu
```

默认需要课程 QEMU 4.1.1，编译器为 `riscv64-unknown-elf-gcc`。
预期看到十行 `100 ticks` 和 `Shutdown after 1000 ticks`，随后 QEMU 正常退出。

现代 QEMU（已验证 10.2.1）使用：

```sh
make clean
make PORTABLE_BOOT=1
make qemu PORTABLE_BOOT=1 QEMU=/usr/bin/qemu-system-riscv64
```

Buddy 与原有独立内存测试：

```sh
make clean
make PMM=buddy MEMBER3_TESTS=1
make qemu
```

`PMM`、`PORTABLE_BOOT`、`MEMBER3_TESTS` 均为编译期开关，切换前必须清理。
默认 `SHUTDOWN_AFTER=10` 指输出组数；每组 100 次时钟中断。
可在干净构建时通过 `DEFS=-DSHUTDOWN_AFTER=...` 覆盖。

## 复核

```sh
python3 tests/check_contracts.py  # 原始框架与 Lab2 保护函数
make grade                      # 原始官方评分，限课程 QEMU 启动路径
python3 tests/run_checks.py      # 五种隔离构建、真实运行、自动关机
python3 tests/run_gdb.py         # 默认课程配置，使用本机 TCP 1235
python3 tests/summarize_results.py
```

`make grade` 会清理当前构建，但仅在临时副本注入测试。之后 `make` 可重新构建默认内核。
GDB 脚本应在默认课程配置的干净构建下运行；手动开 QEMU 时也可使用
`make debug`，另一个终端运行 `make gdb GDB=gdb-multiarch`。

截图复跑依赖 Xvfb、xterm 和 Pillow：

```sh
python3 tests/capture_evidence.py
# 若工具仅解包到某目录，可加 --runtime /path/to/extracted/root
```

截图使用私有 X11 显示器和真实终端帧缓冲。`screenshots/04_extra_test_logs.png`
是查看已保存的测试证据，其他三张为实际运行评分、QEMU、GDB 的截图。

## 目录

| 路径 | 内容 |
|---|---|
| `kern/trap/` | 两个本次编程模块 |
| `kern/mm/` | 沿用 Lab2 实现与 Lab3 中断保护包装 |
| `specs/`、`prompts/` | 约束、来源摘要与实现/验证输入 |
| `tools/` | 官方原始注入和评分代码 |
| `tests/`、`debug/` | 额外集成测试、框架保留校验、GDB 检查 |
| `logs/`、`screenshots/` | 本次原始证据和截图 |
| `reference/lab3_original.zip` | 课程原始代码包，用于验证未改评分代码 |

单 hart 教学实现；未实现完整用户态切栈、进程调度、设备服务、多核同步或通用缺页恢复。

迁移说明：本目录来自 MonsterN58/10-OS2026 的 labcodes/lab3；实验报告经过改写，logs/artifact_sha256.json 保存源工程的历史摘要。
