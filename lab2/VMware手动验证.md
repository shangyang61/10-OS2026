# VMware 中手动验证 Lab2

本流程在 VMware 的 Ubuntu 内直接操作，不依赖主机 SSH。当前用户虚拟机已能从主机 ping 通，但 SSH 22 端口未监听；这不妨碍在 Ubuntu 终端编译和运行 QEMU。

当前交付的整合版本尚未在这台 VMware 内得到通过日志。下面是待执行步骤，已有 WSL 结果保存在另一个明确命名的日志组中。

## 1 把代码放进 Ubuntu

将交接压缩包复制到 Ubuntu，用文件管理器解压。可尝试 VMware Tools 提供的拖放或共享文件夹；若复制不便，也可在 Ubuntu 浏览器打开以下分支，点击 Code → Download ZIP 后解压：

https://github.com/shangyang61/10-OS2026/tree/lab2-member2-report

交接包内的源码目录为 `lab2_member2_2412592/lab2`。GitHub 分支压缩包解压后，进入其中的 lab2 目录。

在该 lab2 文件夹内右键选择“在终端打开”。如果没有这个选项，先打开终端，输入 `cd `，把 lab2 文件夹拖进终端，再按回车。

```sh
pwd
ls Makefile kern/mm/best_fit_pmm.c kern/mm/slub.c tests/run_checks.py
```

pwd 输出应以 /lab2 结尾，ls 应能列出这四个路径。不要在 Windows PowerShell 中执行后面的 Linux 构建命令。

## 2 确认工具和版本

```sh
uname -a
python3 --version
gcc --version | head -n 1
make --version | head -n 1
command -v riscv64-unknown-elf-gcc
command -v qemu-system-riscv64
```

课程工具链常放在 /opt/riscv/bin。若该目录确实存在，可先执行：

```sh
export PATH=/opt/riscv/bin:$PATH
```

如果仍找不到工具，可以查找本机已有的可执行文件：

```sh
find "$HOME" /opt /usr/local -type f \( -name riscv64-unknown-elf-gcc -o -name qemu-system-riscv64 \) 2>/dev/null
```

将实际工具链 bin 目录和 QEMU 可执行文件所在目录加入 PATH 后，再检查版本：

```sh
riscv64-unknown-elf-gcc --version | head -n 1
LAB_QEMU="$(command -v qemu-system-riscv64)"
"$LAB_QEMU" --version | head -n 1
```

LAB_QEMU 必须是本机真实可执行路径。若工具根本不存在，将这些检查输出交给组内环境维护者补齐工具；此时还没有开始内核测试。

成员一原机器的 QEMU 路径为 `/home/vmuser/src/qemu-4.1.1/riscv64-softmmu/qemu-system-riscv64`，只在本机确实存在该文件时使用，不能照搬其他机器的用户名和目录。

## 3 按实际 QEMU 选择一种启动配置

| 实际环境 | 选择 |
|---|---|
| 成员一使用的课程 QEMU 4.1.1 / 原课程固件 | course，PORTABLE_BOOT=0 |
| 现代 QEMU/OpenSBI，例如已有 WSL 验证使用的 QEMU 7 | portable，PORTABLE_BOOT=1 |

课程配置保留 loader 和单个高地址根映射；portable 配置使用 -kernel 并增加启动过渡同址映射。不能只改启动命令而继续复用另一种配置的旧对象。自动测试运行器使用干净临时目录，已避免这一混用问题。

课程环境执行：

```sh
python3 tests/run_checks.py --profile course --qemu "$LAB_QEMU" --grade --first-fit --log-prefix member3_vm_course
```

现代环境执行：

```sh
python3 tests/run_checks.py --profile portable --qemu "$LAB_QEMU" --grade --first-fit --log-prefix member3_vm_portable
```

两条命令按环境选择一条。这里是在验证当前成员二基线，尚不是成员三新写的 Buddy 测试。运行器会构建、启动 RISC-V 内核、采集输出并结束 QEMU，最后写入日志。

## 4 对照结果并保存日志

成功时终端应出现 normal、original make grade 和 first_fit 的 PASS，以及 All requested checks passed。原始评分日志应显示：

```text
  -check page init:                          OK
  -check best fit:                           OK
Total Score: 20/20
```

正常内核日志应包含：

```text
memory management: best_fit_pmm_manager
check_alloc_page() succeeded!
member2_page_check() succeeded!
slub_check() succeeded!
```

实际成功行后面可能附带说明文字。还需确认没有 panic、assertion failed 或 FAIL；不能只看到某一条 succeeded 就认定整个流程完成。

新文件按所选前缀写到 logs，例如：

```text
member3_vm_course_128M_environment.json
member3_vm_course_128M_build.log
member3_vm_course_128M_qemu.log
member3_vm_course_128M_grade.log
member3_vm_course_128M_first_fit_qemu.log
member3_vm_course_128M_results.json
```

还会保存保护函数和初始化边界日志。portable 的文件名前缀相应变化。将这些文件一并交给成员三，在报告中填写这台 VMware 的实际版本和结果，保留旧 WSL 与成员一历史日志。

## 5 想在终端亲自观察内核输出

现代配置示例：

```sh
make clean
make -j2 PORTABLE_BOOT=1
make qemu PORTABLE_BOOT=1 QEMU="$LAB_QEMU"
```

课程配置将两处 PORTABLE_BOOT 改为 0。看到成功信息后，内核末尾 while(1) 会继续运行，这不是程序卡死的证据。退出 QEMU 的 nographic 会话时，先按 Ctrl+A，松开后按 X。若键盘被 VMware 捕获，Ctrl+Alt 可将输入释放回 Windows。

## 6 常见现象

| 现象 | 下一步 |
|---|---|
| command not found | 检查工具文件是否存在及 PATH；此时尚未进行有效构建 |
| 只有 OpenSBI 输出，没有内核成功标记 | 对照 QEMU/固件与启动配置，先清理旧对象再手动观察 |
| assertion failed 或 panic | 保留完整日志、实际命令与版本，定位断言；不修改原始测试来绕过 |
| 窗口停在成功输出后 | 当前内核会永久循环；自动运行器会主动结束采集 |
| 换成 Buddy 后旧自检失败 | 先核对交接说明中的算法契约和全池分配假设，使用成员三独立测试入口 |

成员三接入 Buddy 后，应另建自己的运行记录与覆盖说明；不要把这里的 Best-Fit 评分当成 Buddy 的验收。
