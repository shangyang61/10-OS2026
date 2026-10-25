# 实验报告实际截图与来源

本目录收录邹俊轩于 2026-10-25 在本人 VMware Ubuntu 终端实际操作后提供的 8 张 PNG。原图来自 Windows 的 `lab2_new/截图/`，只按用途重命名并逐字节复制，没有重绘、拼接、修改文字或处理像素。图片已直接插入[实验报告](../实验报告.md)。

| 报告图号 | 仓库文件 | 原始文件名 | 内容 | 像素尺寸 |
|---|---|---|---|---|
| 图1a | [S01_environment_overview.png](S01_environment_overview.png) | `0.png` | 本机路径、系统和 QEMU 位置 | 1094×290 |
| 图1b | [S01_tool_versions.png](S01_tool_versions.png) | `version.png` | GCC、QEMU、Python、Make 版本 | 1092×440 |
| 图2 | [S05_page_init_gdb.png](S05_page_init_gdb.png) | `5.png` | slub_init 断点后的初始化与根表 | 572×601 |
| 图3a | [S09_bare_translation.png](S09_bare_translation.png) | `91.png` | Bare 取指调用栈和翻译返回 | 1235×714 |
| 图3b | [S09_bare_tlb.png](S09_bare_tlb.png) | `92.png` | Bare 软件 TLB 填充参数 | 1234×722 |
| 图4a | [S02_checks_summary.png](S02_checks_summary.png) | `firstfit.png` | Best-Fit、20/20 与 First-Fit 回归摘要 | 1104×156 |
| 图4b | [S02_preserved_boundaries.png](S02_preserved_boundaries.png) | `1.png` | 五保护函数、10/10 边界 | 1006×421 |
| 图4c | [S03_integration_summary.png](S03_integration_summary.png) | `buddy.png` | Buddy / Best-Fit 两策略集成摘要 | 1108×135 |

图4a、图4c 是运行器汇总截图，不是逐项测试输出；详细覆盖说明和可下载原始日志见报告第十节。图4b 的末尾命令含真实粘贴残留，图注只使用此前已完成的保护与边界输出。图2是在客体 GDB 连接并停到 slub_init 后取得的有效现场，不能与早先未连接时的零初值混用。

图3a/3b 证明 Bare 取指直接返回相同物理地址后仍进入 QEMU 软件 TLB 填充。Sv39 页表查询及后续 TLB 命中使用成员一已提交的 `logs/os2026_host_gdb.log`，没有为 S06～S08 生成替代截图。新运行的 `my_vm_*` 完整日志仍在本人虚拟机内，不能将仓库现有成员三日志改名冒充新日志。

原图与仓库文件的 SHA256 已核对一致：

```text
d72d4a508ee12f3728cd44483d680e9c7a7303c263a6b81f372905deec7c3a7c  S01_environment_overview.png
39d82754abdd3bf0e05885996fa2f8a7f2de4a44861ef0a5199360ff3dbf1cc6  S01_tool_versions.png
58c87d92ffa5de73b925f376a8750ca446b581a4b85c0fbebf23699a99301166  S05_page_init_gdb.png
cf95addef4485a3ff40de762fca7ef3bb466500b719c0dcadf2d3e297c0449e1  S09_bare_translation.png
837959faf65fe654845d91fb646f253776b981e9018db6175c3980d861f5c332  S09_bare_tlb.png
f16fed6311022f942e1a888b4977e29b57b02d20dcd5ba6cdc86aa66a6a6f5b6  S02_checks_summary.png
b3725af89fe28326ff2333999fbf87178d2060a3b9d360cdae5440b20358595f  S02_preserved_boundaries.png
28136e56ae2f18c5682b8b313272855d8765bba786db9a366a4d57549efd1d56  S03_integration_summary.png
```

PNG 由仓库 `.gitattributes` 标为 binary，避免 Git 的文本换行转换损坏图片。复现命令与调试过程见[实验报告](../实验报告.md)第八、十节。
