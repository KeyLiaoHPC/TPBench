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
| A3.6 | `corelib/strftime.h` | 验证时间结构体经 `tpb_dtbits_t` 位压缩往返后字段值保持不变（含编码/解码子路径）。 |
| A3.12 | `corelib/strftime.h` | 验证位压缩经 ISO 8601 UTC 字符串往返后值不变（含 `bits_to_isoutc`/`isoutc_to_bits`）。 |
| A3.13 | `corelib/strftime.h` | 验证位压缩经 ISO 8601 时区字符串往返后值不变（含 `+08:00` 等偏移格式）。 |
| A3.14 | `corelib/strftime.h` | 验证 strftime 模块对非法模式参数返回错误。 |
| A3.15 | `corelib/strftime.h` | 验证 strftime 模块所有函数拒绝 NULL 指针参数。 |
| A3.16 | `corelib/strftime.h` | 验证 strftime 模块支持 1970–2225 年份范围，越界返回错误。 |
| A3.17 | `corelib/strftime.h` | 验证 strftime 模块支持 −720 至 +720 分钟时区偏移往返。 |
| A4.3 | `corelib/rafdb/rafdb-l1-magic.c` | 验证 RAFDB 魔术字节构造、有效校验及损坏/跨域拒绝（合并原 magic 正反例）。 |
| A4.9 | `corelib/rafdb/rafdb-l2-*.c` | 验证 RAFDB tbatch/kernel/task ID 确定性、哈希一致性与不同输入产生不同 ID。 |
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
| A4.21 | `corelib/rafdb/rafdb-l2-rtenv.c` | 验证 RTEnv ID 单调分配（含 base 环境占用 id=1）。 |
| A4.22 | `corelib/rafdb/rafdb-l2-rtenv.c` | 验证 RTEnv 重名 entry 追加被拒绝。 |
| A4.23 | `corelib/rafdb/rafdb-l2-rtenv.c` | 验证 RTEnv entry 写入后 list 可正确读回。 |
| A4.24 | `corelib/rafdb/rafdb-l2-rtenv.c` | 验证 RTEnv record 属性与载荷往返读写。 |
| A4.25 | `corelib/rafdb/rafdb-l2-rtenv.c` | 验证 RTEnv 十进制 ID 路径查找与 record 定位。 |
| A4.26 | `corelib/rafdb/rafdb-l2-rtenv.c` | 验证 RTEnv `ntask`/`ntbatch` 计数 patch 同步至 entry 与 record。 |
| A4.27 | `corelib/rafdb/rafdb-l2-rtenv.c` | 验证 RTEnv derive 链接追加与 `DeriveTo` header 更新。 |
| A4.28 | `corelib/rafdb/rafdb-l2-rtenv.c` | 验证 `base_id` 配置读写及 `rafdb_config.json` 持久化。 |
| A4.29 | `corelib/tpb-rtenv.c` | 验证 `TPB_RTENV_ID` 解析、无效 ID 回退与 active 环境选择。 |
| A4.30 | `corelib/tpb-rtenv.c` | 验证从进程环境构建 RTEnv 快照 record（key/count/value 编码）。 |
| A5.3 | `corelib/tpb-autorecord.c` | 验证 `tpb_k_write_task` 可完成 int64 数组往返读写。 |
| A5.6 | `corelib/tpb-autorecord.c` | 验证 `tpb_k_write_task` 可同时输出 double、int32、float 三个数组（含各标量类型往返）。 |
| A5.7 | `corelib/tpb-autorecord.c` | 验证 `tpb_k_write_task` 跳过未分配（n=0, p=NULL）的输出项。 |
| A5.8 | `corelib/rafdb/rafdb-l1-record-io.c` | 验证 `tpb_raf_header_data_ptr` 偏移计算、越界与短 blob 错误处理。 |
| A6.4 | `corelib/rafdb/rafdb-l3-task-taglink.c`, `rafdb-l2-task.c` | 验证 capsule record 包含正确 header（`name=TaskID`, `tag=TPBLINK`）和 20 字节载荷；覆盖 task ID 与 capsule ID 区分及 NULL 输出指针路径。 |
| A6.5 | `corelib/rafdb/rafdb-l2-task.c` | 验证 capsule entry 出现在 `task.tpbe` 列表中。 |
| A6.6 | `corelib/rafdb/rafdb-l3-task-taglink.c` | 验证单条 task 可追加至 capsule，载荷为 40 字节。 |
| A6.7 | `corelib/rafdb/rafdb-l3-task-taglink.c` | 验证三条 task 追加至 capsule，载荷为 80 字节且顺序保持。 |
| A6.8 | `corelib/rafdb/rafdb-l3-task-taglink.c`, `rafdb-l1-entry.c` | 验证两个独立进程可分别追加 task 至同一 capsule（跨 PID 锁）。 |
| A6.9 | `corelib/tpb-autorecord.c` | 验证 `tpb_k_sync_capsule_task` 返回与创建时相同的 capsule ID。 |
| A7.1 | `corelib/rafdb/rafdb-l2-kernel-meta-build.c` | 验证 `tpb_raf_kernel_build_registered_attr` 正确统计 parm 和 metric 数量；`nheader` 为下限（含固定 meta），必选 meta header 按名校验。 |
| A7.2 | `corelib/tpb-autorecord.c`, `kernels/simple/tpbk_stream.c` | 验证真实 stream kernel 注册后 record 包含 ≥3 个 parm 和 ≥4 个 metric；`nheader` 为下限，variation/compilation/dependency 按名存在。 |
| A8.1 | `corelib/tpblog/tpb-printf.h` | 验证 tpblog 初始化后日志文件包含 "TPBench Run Log" 和会话头。 |
| A8.2 | `corelib/tpblog/tpb-printf.h` | 验证 tpblog 重新初始化时追加内容且不重写文件头。 |
| A8.3 | `corelib/tpblog/tpb-printf.h` | 验证 tpblog 输出同时出现在 stdout 和日志文件中（含 `tpblog_snprintf` 宏格式化）。 |
| A8.4 | `corelib/tpblog/tpb-printf.h` | 验证 tpblog WARN 消息标记 `[WARN]` 标签而非 `[NOTE]`。 |
| A8.5 | `corelib/tpblog/tpb-printf.h` | 验证 `_tpblog_compute_column_widths(24, {1,2,3})` 计算结果为 `{2,5,9}`。 |
| A8.6 | `corelib/tpblog/tpb-printf.h` | 验证 tpblog 列格式化输出包含预期的单元格文本。 |
| A8.7 | `corelib/tpblog/tpb-printf.h` | 验证 tpblog 在 10 字符宽度下渲染 8 列仍显示所有单元格。 |
| A8.10 | `corelib/tpblog/tpb-printf.h` | 验证 `TPB_FAIL`/`TPB_PROPAGATE` 日志标签与 `[errcode=]` 输出（含 tag 宏路径）。 |
| A8.11 | `corelib/tpblog/tpb-printf.h` | 验证 `ctab` 最小列宽约束。 |
| A8.12 | `corelib/tpblog/tpb-printf.h` | 验证 `ctab` 右对齐列渲染。 |
| A8.13 | `corelib/tpblog/tpb-printf.h` | 验证 `ctab` 14 列宽表格完整输出。 |
| A9.2 | `include/tpb-public.h` | 验证 `TPBE_MAKE`/`TPBE_CAUSE`/`TPBE_MODULE` 编解码及 `tpb_err_propagate` 模块替换。 |
| A9.3 | `include/tpb-public.h` | 验证 `tpb_err_propagate` 对裸 cause 码的兼容处理。 |
| A9.4 | `include/tpb-public.h` | 验证 `tpb_err_to_exit_status` 仅取 cause；`TPBE_SUCCESS`/`TPBE_EXIT_ON_HELP` 不编码。 |
| A9.5 | `include/tpb-public.h` | 验证未知 cause 的 `tpb_err_to_string` 回退消息。 |
| A9.6 | `include/tpb-public.h` | 验证 `tpb_err_module_name` 返回可读模块名。 |
| A10.1 | `corelib/tpb-rtenv.c` | 验证环境快照单 key 编码（`:` 连接 key/value 段）与 count 解码往返。 |
| A10.2 | `corelib/tpb-rtenv.c` | 验证 PATH 式多段 value（`count=2`）扁平存储与解码拼回。 |
| A10.3 | `corelib/tpb-rtenv.c` | 验证多 key 混合段数时按 key→count→value 顺序解码。 |
| A10.4 | `corelib/tpb-rtenv.c` | 验证空 value（`count=0`）不消费 value 段。 |
| A10.5 | `corelib/tpb-rtenv.c` | 验证 value 段内可含 `;`（连接符仅为 `:`）。 |
| A11.1 | `corelib/tpb-tag-norm.c` | 验证 tag 去重/大写/排序及系统附加 `TPBOUTPUT`/`TPBINPUT`（含展示格式与幂等性）。 |
| A11.2 | `corelib/tpb-tag-norm.c` | 验证 name/tag 合法性（禁止 `:`、长度上限）。 |
| A11.5 | `include/tpb-public.h` | 验证六个预设 tag 宏字符串。 |
| A11.6 | `corelib/tpb-tag-norm.c` | 验证角色 tag 与用户 tag 去重组合。 |
| A12.1 | `utils/tpb-elf-export.c` | 验证 `libdep.so` 含强 `U` 且 plugin `DT_NEEDED` 它时，`audit-needed` 非零退出，stderr 含 `undefined symbol` 与 `libdep`。 |
| A12.2 | `utils/tpb-elf-export.c` | 验证 `classify-dso` 根据 `.comment` / `libirc` 输出 `gcc` / `clang_rt` / `intel` 之一。 |
| A12.3 | `utils/tpb-elf-export.c` | 验证去掉原 `.comment` 后，写入 Intel / clang 注释的两个 DSO 分类族不同。 |

## B 类 — CLI 单元与功能测试

针对 `src/tpbcli/` 命令行前端各子模块的功能测试。通过 fork/exec 调用真实 `tpbcli` 二进制、直接编译 argp 源码或检查 dry-run 输出，验证参数解析、子命令行为、错误提示和 wrapper 链组装。

| 编号 | 涉及源代码模块 | 测试目的 |
|------|---------------|----------|
| B1.1 | `tpbcli/run/tpbcli-run-dim.c` | 验证维度参数解析器可将 `[16,32,64]` 展开为 3 个数值。 |
| B1.2 | `tpbcli/run/tpbcli-run-dim.c` | 验证维度参数解析器支持递归生成 `mul(@,2)(16,16,128,0)` → `16,32,64,128`。 |
| B1.3 | `tpbcli/run/tpbcli-run-dim.c` | 验证维度参数解析器可将 `[double,float,iso-fp16]` 解析为字符串列表。 |
| B1.4 | `tpbcli/run/tpbcli-run-dim.c` | 验证维度参数解析器拒绝嵌套花括号语法。 |
| B2.1 | `tpbcli/run/tpbcli-run.c` | 验证 `run` 子命令禁止 `--kargs`/`--kargs-dim`/`--kenvs`/`--kenvs-dim` 出现在 `--kernel` 之前。 |
| B2.5 | `tpbcli/run/tpbcli-run.c`, `kernels/simple/tpbk_stream.c` | 验证 stream kernel 正常执行且 Triad 带宽 > 0；重复运行显示 KernelID 且无 "already recorded" 警告。 |
| B2.11 | `tpbcli/run/tpbcli-run.c` | 验证不存在 kernel 显示 "not found" 提示且不触发 dynloader 扫描错误。 |
| B2.12 | `tpbcli/run/tpbcli-run.c` | 验证干跑模式下两个 kargs-dim 产生 4 条 Exec 行（2×2 笛卡尔积）。 |
| B2.13 | `tpbcli/run/tpbcli-run.c` | 验证 `run --kernel stream --help` 显示 Parameters 与统一 Data Records 表（含 `TPBINPUT`/`TPBOUTPUT`/`TPBFOM`）。 |
| B2.15 | `tpbcli/run/tpbcli-run.c`, `corelib/rafdb/` | 验证只读 task_batch 目录导致 run 报 "begin_batch failed" 错误。 |
| B3.1 | `tpbcli/argp/tpbcli-argp.c` | 验证 argp 树创建、添加、销毁生命周期；重复名称被拒绝；兄弟链正确。 |
| B3.2 | `tpbcli/argp/tpbcli-argp.c` | 验证 argp 解析器可解析 `run --kernel stream --kargs n=10` 并触发回调。 |
| B3.3 | `tpbcli/argp/tpbcli-argp.c` | 验证 argp 解析器缺失必选参数报错；预设值填充缺失可选参数。 |
| B3.6 | `tpbcli/argp/tpbcli-argp.c` | 验证 argp 解析器 `-h` 分发到正确的深度级回调。 |
| B3.7 | `tpbcli/argp/tpbcli-argp.c` | 验证 argp 解析器对未知 `--bogus` 参数返回 `TPBE_CLI_FAIL`。 |
| B3.8 | `tpbcli/argp/tpbcli-argp.c` | 验证 argp 解析器在选项归属不同父作用域时可回退重试。 |
| B3.9 | `tpbcli/argp/tpbcli-argp.c` | 验证 argp 解析器废弃选项（max_chosen=0）报错；互斥 max_chosen 限制生效（含 `-P`/`-F` 冲突）。 |
| B3.11 | `tpbcli/argp/tpbcli-argp.c` | 验证 argp 解析器 DELEGATE_SUBCMD 标志将未解析参数传递给回调（含深度约束搜索）。 |
| B4.1 | `tpbcli/database/tpbcli-database.c` | 验证裸 `database` 命令失败并提示 list/dump 子命令。 |
| B4.2 | `tpbcli/database/tpbcli-database.c` | 验证 `database -h` 显示 list/dump 及 `-dT`/`-i`/`-e` 说明（含 `list -h`/`dump -h` 上下文）。 |
| B4.5 | `tpbcli/database/tpbcli-database.c` | 验证 `database dump` 无 domain 时报错。 |
| B4.6 | `tpbcli/database/tpbcli-database.c` | 验证 `database dump -dT -i X -e` 报告 "conflict" 冲突（含 `-i`/`-e` 互斥）。 |
| B4.7 | `tpbcli/database/tpbcli-database.c` | 验证 `database nosuchcmd` 报 "unknown argument" 错误。 |
| B4.9 | `tpbcli/database/tpbcli-database.c` | 验证 `database ls` 作为 `list` 别名成功执行。 |
| B4.10 | `tpbcli/database/tpbcli-database-ls.c` | 验证 `database list -dt` 正常退出且含 task 表头。 |
| B4.11 | `tpbcli/database/tpbcli-database-ls.c` | 验证 `database list --domain kernel` 正常退出且含 kernel 表头。 |
| B4.12 | `tpbcli/database/tpbcli-database.c` | 验证 `database list -n 3 -N 3` 报告 count 选项冲突。 |
| B4.13 | `tpbcli/database/tpbcli-database.c` | 验证 `database list -dT -dk` 报告 domain 选项冲突。 |
| B4.14 | `tpbcli/database/tpbcli-database.c` | 验证 `database list --domain bogus` 报告未知 domain 错误。 |
| B4.15 | `tpbcli/database/tpbcli-database-ls.c` | 验证 `database list -dr` 与 `--domain runtime_environment` 含 rtenv 表头（含 `-dT -dr` 冲突）。 |
| B4.18 | `tpbcli/database/tpbcli-database.c` | 验证 `database dump -i <id>` 无 domain 时报错。 |
| B4.19 | `tpbcli/database/tpbcli-database.c` | 验证 `database dump -dT` 无 `-i`/`-e` 时报错。 |
| B4.22 | `tpbcli/database/tpbcli-database-dump.c` | 验证 `database dump -dr -e` 输出 rtenv `.tpbe` 头。 |
| B4.23 | `tpbcli/database/tpbcli-database-dump.c` | 验证 `database dump -dr -i 1` 输出 rtenv `.tpbr` 可读布局（Section/magic/KV `=`/END OF FILE）；Record Data 中 `key[0]` 为可读 STRING（非逐字节 hex）。 |
| B5.2 | `tpbcli/kernel/`, `corelib/rafdb/` | 验证 `kernel get` 不修改 kernel.tpbe entry 数量。 |
| B5.3 | `tpbcli/kernel/`, `corelib/rafdb/` | 验证 `kernel set` 后 `get -v` 显示 Parameters + Data Records（含预设 tag），无旧 Metrics/type 包装。 |
| B5.6 | `tpbcli/kernel/` | 验证 `kernel build`/`set`/`init` 缺少参数时失败并显示用法提示。 |
| B5.7 | `tpbcli/kernel/` | 验证 `kernel build --kernel` 和 `--kernel-tag` 同时使用报互斥错误。 |
| B5.9 | `tpbcli/kernel/` | 验证 `kernel build` 使用未知 tag 时报 "no kernels matched" 错误。 |
| B5.10 | `tpbcli/kernel/` | 验证 `kernel build` 缺参用法含 `--static-libs`，且不含 `--no-auto-runtime`。 |
| B5.11 | `tpbcli/kernel/` | 验证 `kernel build --static-libs` 缺少取值时失败。 |
| B6.2 | `tpbcli/rtenv/` | 验证 `rtenv --help` 概览及缺子命令用法提示。 |
| B6.3 | `tpbcli/rtenv/` | 验证 `rtenv new` 输出 `name=`/`var=` 模板行。 |
| B6.4 | `tpbcli/rtenv/` | 验证 `rtenv new -f` 从文件创建记录。 |
| B6.5 | `tpbcli/rtenv/` | 验证 `rtenv new` 内联 `name=`/`var=` 参数解析。 |
| B6.6 | `tpbcli/rtenv/` | 验证 `rtenv new` 非法模板行被拒绝。 |
| B6.7 | `tpbcli/rtenv/` | 验证 `rtenv list` 显示已激活环境。 |
| B6.8 | `tpbcli/rtenv/` | 验证 `rtenv show` 定列宽与 `On_set`/`On_get` 合并表。 |
| B6.9 | `tpbcli/rtenv/` | 验证 `rtenv load` 按 `on_set` 输出 export shell 片段。 |
| B6.10 | `tpbcli/rtenv/` | 验证 `rtenv new` 需要 active 环境上下文。 |
| B7.2 | `tpbcli/task/` | 验证 `task --help` 概览含 ls / get-result / export（含缺子命令提示）。 |
| B7.4 | `tpbcli/task/` | 验证 `list` 别名等价于 `ls`。 |
| B7.5 | `tpbcli/task/` | 验证空库 `ls` 退出 0 且不更新 RIDMAP。 |
| B7.6 | `tpbcli/task/` | 验证只列入口（独立任务 + capsule）。 |
| B7.7 | `tpbcli/task/` | 验证 `-n`/`-N`/`0` 数量限制（含 `-n`/`-N` 冲突）。 |
| B7.8 | `tpbcli/task/` | 验证 `exit_code` 过滤器。 |
| B7.9 | `tpbcli/task/` | 验证非法过滤 key/operator 被拒绝。 |
| B7.10 | `tpbcli/task/` | 验证本地时间表头与带偏移输出格式。 |
| B7.11 | `tpbcli/task/` | 验证 capsule `Subproc` 计数。 |
| B7.12 | `tpbcli/task/` | 验证 RIDMAP 按显示顺序原子更新。 |
| B7.13 | `tpbcli/task/` | 验证零结果保留旧 RIDMAP。 |
| B7.15 | `tpbcli/task/` | 验证 `kernel_id` 前缀过滤。 |
| B7.20 | `tpbcli/task/` | 验证 `--data-name` CSV 引号/逗号解析。 |
| B7.21 | `tpbcli/task/` | 验证 6–20 hex 前缀解析。 |
| B7.22 | `tpbcli/task/` | 验证 capsule 原始样本池化统计。 |
| B7.23 | `tpbcli/task/` | 验证全部指标缺失返回 `TPBE_METRIC_MISSING`。 |
| B7.24 | `tpbcli/task/` | 验证成员 `-i` 经 `derive_to` 跟随到入口。 |
| B7.25 | `tpbcli/task/` | 验证 `gr` 别名。 |
| B7.26 | `tpbcli/task/` | 验证仅 meta 模式与 UTC Datetime。 |
| B7.27 | `tpbcli/task/` | 验证仅 data 模式。 |
| B7.29 | `tpbcli/task/` | 验证 `--show-each-subrank`。 |
| B7.30 | `tpbcli/task/` | 验证重复 output 名告警。 |
| B7.31 | `tpbcli/task/` | 验证 capsule 全部成员重复 output 名时返回 `TPBE_METRIC_MISSING`。 |
| B7.32 | `tpbcli/task/` | 验证跨成员 unit/shape 不一致时跳过并告警。 |
| B7.34 | `tpbcli/task/` | 验证部分成员缺少指定 data name 时的 `used N/M members` 告警。 |
| B7.35 | `tpbcli/task/` | 验证 tbatch 记录缺失时 `batch_host` 显示 `N/A` 并告警。 |
| B7.36 | `tpbcli/task/` | 验证全部 `task_attr_t` 标量 meta key。 |
| B7.37 | `tpbcli/task/` | 验证 `header[INDEX].FIELD` meta key 及非法 key 提前失败。 |
| B7.38 | `tpbcli/task/` | 验证 `--meta-name --help` 的 Shared/Private 名称报告（含 `--data-name --help` 上下文）。 |
| B7.40 | `tpbcli/task/` | 验证独立任务导出 meta/data 成对文件。 |
| B7.41 | `tpbcli/task/` | 验证双逗号 CSV 转义。 |
| B7.42 | `tpbcli/task/` | 验证 `--from-ls`。 |
| B7.43 | `tpbcli/task/` | 验证 capsule 与成员成对导出。 |
| B7.44 | `tpbcli/task/` | 验证 `subrank` 成员过滤。 |
| B7.45 | `tpbcli/task/` | 验证非 TTY 默认导出策略提示。 |
| B7.46 | `tpbcli/task/` | 验证非法 id 前缀失败。 |
| B7.47 | `tpbcli/task/` | 验证 export 成员过滤解析。 |
| B7.48 | `tpbcli/task/` | 验证 export 4–40 hex 前缀。 |
| B7.49 | `tpbcli/task/` | 验证环境三 header 折叠为 data 两列。 |
| W1.1 | `tpbcli/run/tpbcli-run.c`, `tpbcli/pli/` | 验证干跑模式 Exec 行包含 `tpbcli-pli-launcher` 且无 wrapper（含 `[DRY-RUN]`/`Exec:` 标记）。 |
| W1.3 | `tpbcli/run/tpbcli-run.c`, `tpbcli/pli/` | 验证干跑模式 Exec 行链式组合全局 wrapper + per-kernel wrapper + args（含仅全局 wrapper 路径）。 |
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
| C1.4 | `tpbcli/benchmark/` | 验证 benchmark YAML 引用不存在 metric 时退出 0、输出警告且 score 显示 N/A。 |
| C1.5 | `corelib/tpb-autorecord.c`, `corelib/tpb-rtenv.c`, `tpbcli/run/` | 验证 `$TPB_RTENV_ID=1` 时 run 后 RTEnv `ntask`/`ntbatch` 递增且 task 含环境快照 header；独立工作区未设置 `$TPB_RTENV_ID` 时回退基础环境并完成记录。 |
| C1.7 | `corelib/tpb-driver.c`, `corelib/tpb-rtenv.c` | 验证 `--kenvs` 变量出现在 task 环境快照 header 中，且 key 以 `:` 连接、`count` 与 value 段一致。 |
| C3.1 | `corelib/tpb-autorecord.c`, `cmake/TPBenchKernelRegistry.cmake` | 验证 stream kernel 分别以 -O2 和 -O3 编译后 `kernel get -v` 显示 ≥2 个版本行。 |
| C4.2 | `tpbcli/kernel/`, `cmake/TPBenchKernelRegistry.cmake`, `cmake/` | 验证构建树已 stage CMake 包与 kernel 模板；同名模板从两个目录构建后 active 版本可切换，非活跃版本存入 `lib/inactive/`，版本计数正确跟踪。 |
| C5.1 | `tpbcli/kernel/`, `cmake/TPBenchKernelRegistry.cmake` | 验证 `kernel list` 显示 Tags 列和 N/A 状态，多 kernel 构建（staxpy,striad）生成 `.so` 并消除 N/A。 |
| C6.1 | `cmake/`, `corelib/rafdb/` | 验证 `cmake --install` 将 kernel/runtime_environment 元数据同步至空/部分前缀，并尊重 `TPB_INSTALL_RAFDB` 覆盖策略。 |
