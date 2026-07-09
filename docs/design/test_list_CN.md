# TPBench 测试列表

## A 类 — Corelib 单元测试

针对 `src/corelib/` 核心库各子模块的白盒/灰盒单元测试。通过 mock 对象、ELF 符号拦截和临时工作目录隔离被测函数，验证核心 API 的正确性、边界条件和错误处理。

| 编号 | 涉及源代码模块 | 测试目的 |
|------|---------------|----------|
| A2.1 | `corelib/tpb-run-pli.c` | 验证 PLI 运行器对 NULL handle 参数返回 `TPBE_NULLPTR_ARG` 错误。 |
| A2.2 | `corelib/tpb-run-pli.c` | 验证 PLI 运行器在可执行文件缺失时返回 `TPBE_KERNEL_INCOMPLETE`。 |
| A2.3 | `corelib/tpb-run-pli.c` | 验证 PLI 运行器子进程正常退出时返回成功状态。 |
| A2.4 | `corelib/tpb-run-pli.c` | 验证 PLI 运行器正确传递子进程非零退出码。 |
| A2.5 | `corelib/tpb-run-pli.c` | 验证 PLI 运行器在子进程被信号终止时返回非零状态。 |
| A2.6 | `corelib/tpb-run-pli.c` | 验证 PLI 运行器向子进程环境注入 `TPB_KERNEL_ID` 十六进制值。 |
| A3.1 | `corelib/strftime.h` | 验证 strftime 模块获取的 UTC 和本地时间与系统 `date` 一致。 |
| A3.2 | `corelib/strftime.h` | 验证 strftime 模块获取系统启动时间返回合理值。 |
| A3.3 | `corelib/strftime.h` | 验证启动时间可正确转换为日历时间结构体。 |
| A3.4 | `corelib/strftime.h` | 验证时间结构体可正确编码为 `tpb_dtbits_t` 位压缩格式。 |
| A3.5 | `corelib/strftime.h` | 验证位压缩格式可正确解码为带时区偏移的时间结构体。 |
| A3.6 | `corelib/strftime.h` | 验证时间结构体经位压缩往返后字段值保持不变。 |
| A3.7 | `corelib/strftime.h` | 验证位压缩格式可生成合法 ISO 8601 UTC 字符串。 |
| A3.8 | `corelib/strftime.h` | 验证位压缩格式可生成带 UTC 偏移的 ISO 8601 字符串。 |
| A3.9 | `corelib/strftime.h` | 验证位压缩格式可生成带 `+08:00` 偏移的 ISO 8601 字符串。 |
| A3.10 | `corelib/strftime.h` | 验证 ISO 8601 UTC 字符串可正确解析回位压缩格式。 |
| A3.11 | `corelib/strftime.h` | 验证带时区的 ISO 8601 字符串可正确解析回位压缩格式。 |
| A3.12 | `corelib/strftime.h` | 验证位压缩经 ISO 8601 UTC 字符串往返后值不变。 |
| A3.13 | `corelib/strftime.h` | 验证位压缩经 ISO 8601 时区字符串往返后值不变。 |
| A3.14 | `corelib/strftime.h` | 验证 strftime 模块对非法模式参数返回错误。 |
| A3.15 | `corelib/strftime.h` | 验证 strftime 模块所有函数拒绝 NULL 指针参数。 |
| A3.16 | `corelib/strftime.h` | 验证 strftime 模块支持 1970–2225 年份范围，越界返回错误。 |
| A3.17 | `corelib/strftime.h` | 验证 strftime 模块支持 −720 至 +720 分钟时区偏移往返。 |
| A4.1 | `corelib/rafdb/rafdb-l1-magic.c` | 验证 RAFDB `tpb_raf_build_magic()` 构造正确的魔数字节序列。 |
| A4.2 | `corelib/rafdb/rafdb-l1-magic.c` | 验证 RAFDB 有效魔术字节可通过校验。 |
| A4.3 | `corelib/rafdb/rafdb-l1-magic.c` | 验证 RAFDB 损坏或跨域魔术字节无法通过校验。 |
| A4.6 | `corelib/rafdb/rafdb-l2-tbatch.c` | 验证 RAFDB tbatch ID 生成具有确定性。 |
| A4.7 | `corelib/rafdb/rafdb-l2-kernel.c` | 验证 RAFDB kernel ID 等于输入 SHA1 哈希值。 |
| A4.8 | `corelib/rafdb/rafdb-l2-task.c` | 验证 RAFDB task ID 确定性生成且 handle_index 变化时 ID 不同。 |
| A4.9 | `corelib/rafdb/rafdb-l2-*.c` | 验证 RAFDB 不同输入产生不同 ID。 |
| A4.10 | `corelib/rafdb/rafdb-l2-tbatch.c`, `rafdb-l1-entry.c` | 验证 RAFDB 单条 tbatch entry 写入后可正确读回。 |
| A4.11 | `corelib/rafdb/rafdb-l2-kernel.c` | 验证 RAFDB 单条 kernel entry 写入后可正确读回。 |
| A4.12 | `corelib/rafdb/rafdb-l2-task.c` | 验证 RAFDB 单条 task entry 写入后可正确读回。 |
| A4.13 | `corelib/rafdb/rafdb-l2-tbatch.c` | 验证 RAFDB 连续追加 5 条 tbatch entry 后 list 返回全部记录。 |
| A4.14 | `corelib/rafdb/rafdb-l2-tbatch.c` | 验证 RAFDB tbatch record 写入属性与数据载荷后可正确读回。 |
| A4.15 | `corelib/rafdb/rafdb-l2-kernel.c` | 验证 RAFDB kernel record 写入属性与 uint64 载荷后可读回。 |
| A4.16 | `corelib/rafdb/rafdb-l2-task.c` | 验证 RAFDB task record 写入 int64 elapsed 载荷后可读回。 |
| A4.17 | `corelib/rafdb/rafdb-l2-tbatch.c`, `rafdb-l1-record-io.c` | 验证 RAFDB 一维 header（4 元素）写入后可正确读回。 |
| A4.18 | `corelib/rafdb/rafdb-l2-tbatch.c` | 验证 RAFDB 三维 header（2×3×4）写入后可正确读回。 |
| A4.19 | `corelib/rafdb/rafdb-l2-tbatch.c` | 验证 RAFDB 混合维度（1D、1D、2D）三条 header 可共存于单条 record。 |
| A5.1 | `corelib/tpb-autorecord.c` | 验证 `tpb_k_write_task` 可完成 double 数组随机长度往返读写。 |
| A5.2 | `corelib/tpb-autorecord.c` | 验证 `tpb_k_write_task` 可完成 float 数组往返读写。 |
| A5.3 | `corelib/tpb-autorecord.c` | 验证 `tpb_k_write_task` 可完成 int64 数组往返读写。 |
| A5.4 | `corelib/tpb-autorecord.c` | 验证 `tpb_k_write_task` 可完成 int32 数组往返读写。 |
| A5.5 | `corelib/tpb-autorecord.c` | 验证 `tpb_k_write_task` 支持随机数据类型和随机长度往返读写。 |
| A5.6 | `corelib/tpb-autorecord.c` | 验证 `tpb_k_write_task` 可同时输出 double、int32、float 三个数组。 |
| A5.7 | `corelib/tpb-autorecord.c` | 验证 `tpb_k_write_task` 跳过未分配（n=0, p=NULL）的输出项。 |
| A6.1 | `corelib/rafdb/rafdb-l2-task.c` | 验证 taskcapsule ID 与 task ID 哈希值不同。 |
| A6.2 | `corelib/tpb-autorecord.c`, `corelib/rafdb/rafdb-l2-task.c` | 验证 `tpb_record_write_task` 输出非零 TaskRecordID。 |
| A6.3 | `corelib/tpb-autorecord.c`, `corelib/rafdb/rafdb-l2-task.c` | 验证 `tpb_record_write_task` 在输出指针为 NULL 时仍创建 entry。 |
| A6.4 | `corelib/rafdb/rafdb-l3-task-taglink.c`, `rafdb-l2-task.c` | 验证 capsule record 包含正确 header（`TPBLINK::TaskID`）和 20 字节载荷。 |
| A6.5 | `corelib/rafdb/rafdb-l2-task.c` | 验证 capsule entry 出现在 `task.tpbe` 列表中。 |
| A6.6 | `corelib/rafdb/rafdb-l3-task-taglink.c` | 验证单条 task 可追加至 capsule，载荷为 40 字节。 |
| A6.7 | `corelib/rafdb/rafdb-l3-task-taglink.c` | 验证三条 task 追加至 capsule，载荷为 80 字节且顺序保持。 |
| A6.8 | `corelib/rafdb/rafdb-l3-task-taglink.c`, `rafdb-l1-entry.c` | 验证两个独立进程可分别追加 task 至同一 capsule（跨 PID 锁）。 |
| A6.9 | `corelib/tpb-autorecord.c` | 验证 `tpb_k_sync_capsule_task` 返回与创建时相同的 capsule ID。 |
| A7.1 | `corelib/rafdb/rafdb-l2-kernel-meta-build.c` | 验证 `tpb_raf_kernel_build_registered_attr` 正确统计 parm 和 metric 数量。 |
| A7.2 | `corelib/tpb-autorecord.c`, `kernels/simple/tpbk_stream.c` | 验证真实 stream kernel 注册后 record 包含 ≥3 个 parm 和 ≥4 个 metric。 |
| A8.1 | `corelib/tpblog/tpb-printf.h` | 验证 tpblog 初始化后日志文件包含 "TPBench Run Log" 和会话头。 |
| A8.2 | `corelib/tpblog/tpb-printf.h` | 验证 tpblog 重新初始化时追加内容且不重写文件头。 |
| A8.3 | `corelib/tpblog/tpb-printf.h` | 验证 tpblog 输出同时出现在 stdout 和日志文件中。 |
| A8.4 | `corelib/tpblog/tpb-printf.h` | 验证 tpblog WARN 消息标记 `[WARN]` 标签而非 `[NOTE]`。 |
| A8.5 | `corelib/tpblog/tpb-printf.h` | 验证 `_tpblog_compute_column_widths(24, {1,2,3})` 计算结果为 `{2,5,9}`。 |
| A8.6 | `corelib/tpblog/tpb-printf.h` | 验证 tpblog 列格式化输出包含预期的单元格文本。 |
| A8.7 | `corelib/tpblog/tpb-printf.h` | 验证 tpblog 在 10 字符宽度下渲染 8 列仍显示所有单元格。 |
| A8.8 | `corelib/tpblog/tpb-printf.h` | 验证 `tpblog_snprintf` 宏格式化输出正确。 |

## B 类 — CLI 单元与功能测试

针对 `src/tpbcli/` 命令行前端各子模块的功能测试。通过 fork/exec 调用真实 `tpbcli` 二进制、直接编译 argp 源码或检查 dry-run 输出，验证参数解析、子命令行为、错误提示和 wrapper 链组装。

| 编号 | 涉及源代码模块 | 测试目的 |
|------|---------------|----------|
| B1.1 | `tpbcli/run/tpbcli-run-dim.c` | 验证维度参数解析器可将 `[16,32,64]` 展开为 3 个数值。 |
| B1.2 | `tpbcli/run/tpbcli-run-dim.c` | 验证维度参数解析器支持递归生成 `mul(@,2)(16,16,128,0)` → `16,32,64,128`。 |
| B1.3 | `tpbcli/run/tpbcli-run-dim.c` | 验证维度参数解析器可将 `[double,float,iso-fp16]` 解析为字符串列表。 |
| B1.4 | `tpbcli/run/tpbcli-run-dim.c` | 验证维度参数解析器拒绝嵌套花括号语法。 |
| B1.5 | `tpbcli/run/tpbcli-run-dim.c` | 验证维度参数解析器计算笛卡尔积总数正确（single=1, flat=3, recursive=4）。 |
| B2.1 | `tpbcli/run/tpbcli-run.c` | 验证 `run` 子命令禁止 `--kargs` 出现在 `--kernel` 之前。 |
| B2.2 | `tpbcli/run/tpbcli-run.c` | 验证 `run` 子命令禁止 `--kargs-dim` 出现在 `--kernel` 之前。 |
| B2.3 | `tpbcli/run/tpbcli-run.c` | 验证 `run` 子命令禁止 `--kenvs` 出现在 `--kernel` 之前。 |
| B2.4 | `tpbcli/run/tpbcli-run.c` | 验证 `run` 子命令禁止 `--wrapper-args` 在无 `--wrapper` 时使用。 |
| B2.5 | `tpbcli/run/tpbcli-run.c`, `kernels/simple/tpbk_stream.c` | 验证 stream kernel 正常执行且 Triad 带宽 > 0。 |
| B2.6 | `tpbcli/run/tpbcli-run.c` | 验证 `run` 子命令禁止 `--kenvs-dim` 出现在 `--kernel` 之前。 |
| B2.8 | `tpbcli/run/tpbcli-run.c` | 验证 `run -d` 干跑模式输出 `[DRY-RUN]` 和 `Exec:` 行。 |
| B2.9 | `tpbcli/run/tpbcli-run.c` | 验证 `run --help` 退出码为 0 并显示 Usage 和 `--kernel` 说明。 |
| B2.10 | `tpbcli/run/tpbcli-run.c` | 验证 `run --kernel -h` 提示需要合法 kernel 名称。 |
| B2.11 | `tpbcli/run/tpbcli-run.c` | 验证不存在 kernel 显示 "not found" 提示且不触发 dynloader 扫描错误。 |
| B2.12 | `tpbcli/run/tpbcli-run.c` | 验证干跑模式下两个 kargs-dim 产生 4 条 Exec 行（2×2 笛卡尔积）。 |
| B2.13 | `tpbcli/run/tpbcli-run.c` | 验证 `run --kernel stream --help` 显示参数和指标列表。 |
| B2.14 | `tpbcli/run/tpbcli-run.c`, `corelib/rafdb/` | 验证重复运行 stream kernel 显示 KernelID 且不报 "already recorded" 警告。 |
| B2.15 | `tpbcli/run/tpbcli-run.c`, `corelib/rafdb/` | 验证只读 task_batch 目录导致 run 报 "begin_batch failed" 错误。 |
| B3.1 | `tpbcli/argp/tpbcli-argp.c` | 验证 argp 树创建、添加、销毁生命周期；重复名称被拒绝；兄弟链正确。 |
| B3.2 | `tpbcli/argp/tpbcli-argp.c` | 验证 argp 解析器可解析 `run --kernel stream --kargs n=10` 并触发回调。 |
| B3.3 | `tpbcli/argp/tpbcli-argp.c` | 验证 argp 解析器缺失必选参数报错；预设值填充缺失可选参数。 |
| B3.4 | `tpbcli/argp/tpbcli-argp.c` | 验证 argp 解析器互斥命令（`run benchmark`）冲突时报错。 |
| B3.5 | `tpbcli/argp/tpbcli-argp.c` | 验证 argp 解析器 `-P` 和 `-F` 互斥选项冲突时报错。 |
| B3.6 | `tpbcli/argp/tpbcli-argp.c` | 验证 argp 解析器 `-h` 分发到正确的深度级回调。 |
| B3.7 | `tpbcli/argp/tpbcli-argp.c` | 验证 argp 解析器对未知 `--bogus` 参数返回 `TPBE_CLI_FAIL`。 |
| B3.8 | `tpbcli/argp/tpbcli-argp.c` | 验证 argp 解析器在选项归属不同父作用域时可回退重试。 |
| B3.9 | `tpbcli/argp/tpbcli-argp.c` | 验证 argp 解析器废弃选项（max_chosen=0）报错；互斥 max_chosen 限制生效。 |
| B3.10 | `tpbcli/argp/tpbcli-argp.c` | 验证 argp 解析器的深度约束搜索可在正确深度找到节点。 |
| B3.11 | `tpbcli/argp/tpbcli-argp.c` | 验证 argp 解析器 DELEGATE_SUBCMD 标志将未解析参数传递给回调。 |
| B4.1 | `tpbcli/database/tpbcli-database.c` | 验证裸 `database` 命令失败并提示 list/dump 子命令。 |
| B4.2 | `tpbcli/database/tpbcli-database.c` | 验证 `database -h` 显示 list/dump 及 `-dT`/`-i`/`-e` 说明。 |
| B4.3 | `tpbcli/database/tpbcli-database.c` | 验证 `database list -h` 显示 Usage 及 `-dT`/`-dt`/`-dk`/`--domain`/`-n`/`-N` 选项。 |
| B4.4 | `tpbcli/database/tpbcli-database.c` | 验证 `database dump -h` 显示 domain、`-i`、`-e`、`-n`/`-N` 标志。 |
| B4.5 | `tpbcli/database/tpbcli-database.c` | 验证 `database dump` 无 domain 时报错。 |
| B4.6 | `tpbcli/database/tpbcli-database.c` | 验证 `database dump -dT -i X -e` 报告 "conflict" 冲突。 |
| B4.7 | `tpbcli/database/tpbcli-database.c` | 验证 `database nosuchcmd` 报 "unknown argument" 错误。 |
| B4.8 | `tpbcli/database/tpbcli-database.c` | 验证 `database dump --notaflag` 报 "unknown argument" 错误。 |
| B4.9 | `tpbcli/database/tpbcli-database.c` | 验证 `database ls` 作为 `list` 别名成功执行。 |
| B4.10 | `tpbcli/database/tpbcli-database-ls.c` | 验证 `database list -dt` 正常退出且含 task 表头。 |
| B4.11 | `tpbcli/database/tpbcli-database-ls.c` | 验证 `database list --domain kernel` 正常退出且含 kernel 表头。 |
| B4.12 | `tpbcli/database/tpbcli-database.c` | 验证 `database list -n 3 -N 3` 报告 count 选项冲突。 |
| B4.13 | `tpbcli/database/tpbcli-database.c` | 验证 `database list -dT -dk` 报告 domain 选项冲突。 |
| B4.14 | `tpbcli/database/tpbcli-database.c` | 验证 `database list --domain bogus` 报告未知 domain 错误。 |
| B4.15 | `tpbcli/database/tpbcli-database-ls.c` | 验证 `database list -dr` 与 `--domain runtime_environment` 含 rtenv 表头。 |
| B4.16 | `tpbcli/database/tpbcli-database.c` | 验证 `database list -dT -dr` 报告 domain 冲突。 |
| B4.17 | `tpbcli/database/tpbcli-database-dump.c` | 验证 `database dump -dT -i <prefix>` 不报 unknown argument。 |
| B4.18 | `tpbcli/database/tpbcli-database.c` | 验证 `database dump -i <id>` 无 domain 时报错。 |
| B4.19 | `tpbcli/database/tpbcli-database.c` | 验证 `database dump -dT` 无 `-i`/`-e` 时报错。 |
| B4.20 | `tpbcli/database/tpbcli-database.c` | 验证 `database dump -dT -i X -e` 报告 `-i`/`-e` 冲突。 |
| B4.21 | `tpbcli/database/tpbcli-database.c` | 验证 `database dump -dT -dk` 报告 domain 冲突。 |
| B4.22 | `tpbcli/database/tpbcli-database-dump.c` | 验证 `database dump -dr -e` 输出 rtenv `.tpbe` 头。 |
| B4.23 | `tpbcli/database/tpbcli-database-dump.c` | 验证 `database dump -dr -i 1` 输出 rtenv `.tpbr` Metadata 与 Record Data（跳过 `data_size == 0` 的 header）。 |
| B4.24 | `tpbcli/database/tpbcli-database.c` | 验证 `database dump -dk -e -n 3 -N 3` 报告 count 冲突。 |
| B5.1 | `tpbcli/kernel/` | 验证 `kernel set` 缺少参数时失败。 |
| B5.2 | `tpbcli/kernel/`, `corelib/rafdb/` | 验证 `kernel get` 不修改 kernel.tpbe entry 数量。 |
| B5.3 | `tpbcli/kernel/`, `corelib/rafdb/` | 验证 `kernel set` 后 `get -v` 显示 kernel 信息和列名，无旧 type 包装。 |
| B5.4 | `tpbcli/kernel/` | 验证 `kernel init` 缺少参数时失败。 |
| B5.5 | `tpbcli/kernel/`, `cmake/TPBenchKernelRegistry.cmake` | 验证模板 kernel 完整生命周期：init → build → run → get -v。 |
| B5.6 | `tpbcli/kernel/` | 验证 `kernel build` 缺少选择器时报错并显示选择器提示。 |
| B5.7 | `tpbcli/kernel/` | 验证 `kernel build --kernel` 和 `--kernel-tag` 同时使用报互斥错误。 |
| B5.8 | `tpbcli/kernel/` | 验证 `kernel list` 输出包含 Tags 列。 |
| B5.9 | `tpbcli/kernel/` | 验证 `kernel build` 使用未知 tag 时报 "no kernels matched" 错误。 |
| W1.1 | `tpbcli/run/tpbcli-run.c`, `tpbcli/pli/` | 验证干跑模式 Exec 行包含 `tpbcli-pli-launcher` 且无 wrapper。 |
| W1.2 | `tpbcli/run/tpbcli-run.c`, `tpbcli/pli/` | 验证干跑模式 Exec 行仅包含全局 wrapper，不含 per-kernel wrapper。 |
| W1.3 | `tpbcli/run/tpbcli-run.c`, `tpbcli/pli/` | 验证干跑模式 Exec 行链式组合全局 wrapper + per-kernel wrapper + args。 |
| W1.4 | `tpbcli/run/tpbcli-run.c`, `tpbcli/pli/` | 验证 `-og` 标志使 per-kernel wrapper 替换全局 wrapper。 |
| W1.5 | `tpbcli/run/tpbcli-run.c`, `tpbcli/pli/` | 验证多 kernel 干跑生成各自独立的 Exec 行和正确 wrapper 链。 |
| W1.6 | `tpbcli/run/tpbcli-run.c` | 验证 `--wrapper-args` 在无 `--wrapper` 时失败。 |

## C 类 — 端到端集成测试

在真实构建产物和文件系统环境下，通过 shell 脚本或 C 程序驱动 `tpbcli`、`tpbcli-pli-launcher` 和 kernel 共享库，验证完整的运行链路、结果持久化、CMake 包安装和 kernel 注册表管理。

| 编号 | 涉及源代码模块 | 测试目的 |
|------|---------------|----------|
| C1.1 | `corelib/tpb-autorecord.c`, `corelib/rafdb/`, `tpbcli/run/`, `tpbcli/pli/` | 验证三种调用方式（tpbcli r、tpbcli run 带 dim、直接 pli-launcher）产生 2 个 tbatch（ntask=1,3）、共 5 条 task 且 tbatch_id 链接正确。 |
| C1.2 | `corelib/tpb-autorecord.c`, `corelib/rafdb/`, `tpbcli/benchmark/` | 验证 YAML 驱动的 benchmark 产生 1 个 BENCHMARK 类型 tbatch 且 1 条 task 正确链接。 |
| C1.3 | `corelib/rafdb/`, `tpbcli/benchmark/` | 验证只读 task_batch 目录导致 benchmark 报 "begin_batch failed" 错误。 |
| C1.4 | `corelib/rafdb/rafdb-l3-task-taglink.c`, `kernels/streaming_memory_access_mpi/` | **手工/CI 可选：** `tpbcli kernel build stream_mpi` + `mpirun -np 4 --map-by core --bind-to core` 运行后 `db list` 显示 1 条 task capsule（4 rank 聚合）。 |
| C3.1 | `corelib/tpb-autorecord.c`, `cmake/TPBenchKernelRegistry.cmake` | 验证 stream kernel 分别以 -O2 和 -O3 编译后 `kernel get -v` 显示 ≥2 个版本行。 |
| C4.1 | `cmake/`, root `CMakeLists.txt` | 验证 CMake 包安装后 `build/lib/cmake/TPBench/` 包含 Config.cmake 和 Kernel.cmake，模板存在于 `build/etc/cmake/kernel/`。 |
| C4.2 | `tpbcli/kernel/`, `cmake/TPBenchKernelRegistry.cmake` | 验证同名模板从两个目录构建后 active 版本可切换，非活跃版本存入 `lib/inactive/`，版本计数正确跟踪。 |
| C5.1 | `tpbcli/kernel/`, `cmake/TPBenchKernelRegistry.cmake` | 验证 `kernel list` 显示 Tags 列和 N/A 状态，多 kernel 构建（staxpy,striad）生成 `.so` 并消除 N/A。 |
