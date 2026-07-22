# `tpbcli task` 前端设计

## 1. 目标

`tpbcli task` 用来查找、对比和导出 task 记录。它新增三个子命令：

```text
tpbcli [--workspace PATH] task ls
tpbcli [--workspace PATH] task get-result|gr
tpbcli [--workspace PATH] task export
```

三条命令分别解决以下问题：

1. `ls` 按条件查找任务，按开始时间排序，为本次结果分配短编号。
2. `get-result` 从若干逻辑任务中读取元数据和输出指标，在终端中对比。
3. `export` 把任务记录写成便于其它程序读取的文本文件。

实现代码放在 `src/tpbcli/task/`。读取 rafdb 时只调用
`tpb-public.h` 中声明的公开函数，不包含 corelib 或 rafdb 的内部头文件。

本功能不做以下改动：

- 不改变 `.tpbe` 和 `.tpbr` 的磁盘格式。
- 不改变 `tpbcli db` 的参数和输出。
- 不修改 task、tbatch 或 kernel 记录。
- 不从运行日志中提取指标。
- 本版不提供 `duration` 过滤。
- 本版不提供 JSON、TSV 或其它导出格式。

`task` 只会写以下位置：

- `<workspace>/.tmp/tpb_rt_local_ridmap`
- `export` 的输出目录

## 2. 当前记录格式

### 2.1 task 索引与完整记录

task 域位于：

```text
<workspace>/rafdb/task/
```

其中：

- `task.tpbe` 是任务索引。
- `<40位十六进制ID>.tpbr` 是一条完整任务记录。

当前 `task_entry_t` 在磁盘中固定占 232 字节。`task ls` 通过公开函数
`tpb_raf_entry_list_task` 读取它，不直接按字节偏移读取文件。

完整记录包含固定属性、若干 header 和数据区。数据区中的各块按 header
顺序连接。读取某一块时必须调用 `tpb_raf_header_data_ptr`，不能自行相信
文件中的 `metasize`。当前 capsule 追加成员时不会更新 `metasize`，但公开
读取函数能够按实际分隔标记和数据长度正确读取。

### 2.2 入口记录

索引行的 `derive_to` 20 个字节全为零时，该行是入口记录。

入口记录有两类：

- 独立任务，只代表自身。
- capsule，代表一次并行运行及其成员。

成员记录的 `derive_to` 通常指向 capsule，因此不出现在 `task ls` 的默认
结果中。

并行任务仍在写入时，成员可能暂时保持 `derive_to == 0`。此时 `task ls`
可能短暂把成员显示为入口。这是当前写入顺序造成的现象，前端不能仅凭
现有字段判断这次并行运行是否已经全部结束。

### 2.3 capsule 的确认规则

索引行同时满足以下条件时，它是 capsule 候选：

- `inherit_from` 20 个字节全为 `0xFF`
- `derive_to` 20 个字节全为 `0x00`

读取完整记录后，还要检查：

- `ninput == 0`
- `noutput == 0`
- `nheader == 1`
- 唯一 header 的名称是 `TaskID`
- 唯一 header 的标签是 `TPBLINK`
- 数据长度是 20 的整数倍
- `dimsizes[0]` 与数据中的 20 字节 ID 数量一致

全部满足时才确认它是 capsule。这样可以避免以后其它记录也使用全
`0xFF` 的 `inherit_from` 时被误认。

若完整记录可以正常读取，但结构不符合上述规则，则把它当作普通入口。
若记录无法读取或数据长度错误，`ls` 仍显示该索引行，将 `Subproc` 显示
为 `?` 并给出警告。`get-result` 和 `export` 在需要展开它时失败，不猜测
成员。

### 2.4 capsule 成员与 subrank

capsule 的 `TaskID` 数据由连续的 20 字节 TaskRecordID 组成。每个 ID
显示为 40 位小写十六进制字符串。

成员在 `TaskID` 数据中的位置称为 `subrank`：

```text
第一个成员  subrank 0
第二个成员  subrank 1
第三个成员  subrank 2
```

`subrank` 只表示 ID 写入 capsule 的先后位置，不保证等于 MPI rank。

capsule 中的 ID 列表是成员关系的依据。若某个成员的 `derive_to` 没有
指回当前 capsule，前端给出警告，但仍按 capsule 中的位置处理该成员。
这能容忍任务在写入中途被打断的情况。

### 2.5 header 分组

普通任务的 header 按以下范围分组：

```text
[0, ninput)                  输入参数
[ninput, ninput + noutput)   实际写入的输出指标
[ninput + noutput, nheader)  其它记录数据
```

`noutput` 是实际写入记录的输出数量。内核运行时未分配数据或元素数为零
的输出不会写入 header。因此，某个指标已在内核中注册，不代表每条 task
记录都一定含有该指标。

当前普通任务通常在末尾增加三条环境变量 header，所以：

```text
nheader = ninput + noutput + 3
```

读取端不能把这个等式当作永久规则。以后还可以增加其它 header。代码
应按名称查找环境变量 header。

### 2.6 环境变量的实际编码

三条环境变量 header 为：

```text
environment_variable_key
environment_variable_count
environment_variable_value
```

它们的含义如下：

- key 是用 `:` 连接的变量名。
- count 是 `int32_t` 数组，每个元素表示对应变量占用几个 value 段。
- value 是按 key 顺序连接的所有非空 value 段。
- `count[i] == 0` 表示空值，不读取 value 段。

当前采集代码会丢弃由连续冒号、开头冒号和结尾冒号产生的空段。因此这
种编码不能总是恢复原字符串：

```text
a::b  读取后为 a:b
:a    读取后为 a
a:    读取后为 a
```

`export` 展示的是按当前编码能够恢复的值，不得宣称它与
原进程环境逐字节相同。

环境变量数量为零时，count header 的 `dimsizes[0]` 为 0，但磁盘中仍有
一个 4 字节占位值。环境变量读取代码必须接受这种情况。

### 2.7 维度与元素顺序

`dimsizes[0]` 是最内层维度，也是连续数据中变化最快的维度。

数值数组的逻辑元素数为所有有效维度大小的乘积。`ndim == 0` 表示一个
数值。计算乘积时必须检查整数溢出，并核对元素数乘元素大小是否等于
`data_size`。

带 `TPBLINK` 标签的 `TaskID` 是特殊情况。其 `type_bits` 在当前格式中
是 `TPB_UNSIGNED_CHAR_T`，但每个逻辑 ID 占 20 字节，不能按单字节数值
导出。

`TPB_STRING_T` 的最内层维度表示一个字符串格可占的字节数。对于一维
字符串，整块数据是一个字符串。对于多维字符串，外层每个位置各产生
一个字符串格。

### 2.8 当前字段限制

- task 的 `duration` 目前写为 0，索引和完整记录都是如此。
- task 固定属性中没有 hostname。
- 关联的 tbatch 记录有 hostname，但它表示批次前端所在主机，不保证是
  每个并行成员实际运行的主机。
- task 的开始时间由当前写入路径按 UTC 保存。

## 3. 命令公共规则

### 3.1 workspace

`--workspace` 必须放在顶层命令之前：

```bash
tpbcli --workspace /path/to/ws task ls
```

未指定时沿用现有 `TPB_WORKSPACE` 和默认 workspace 解析规则。

### 3.2 ID 输入

`get-result` 和 `export` 都可通过 `-r` 或 `--rid` 读取 RIDMAP 中的短
编号。`get-result` 使用 `-i` 或 `--task-id` 直接选择 task ID；`export`
保留 `-i` 或 `--id`。同一条命令中的两种 ID 来源互斥。

`-r` 支持：

```text
0
0,1,3
0-3
0,2-4,8
```

范围是闭区间。短编号必须是非负十进制整数。

`get-result -i` 支持用逗号分开的 6 到 20 位十六进制前缀：

```text
a1b2c3
a1b2c3,0123456789abcdef
```

这里的长度是十六进制字符数，不是字节数。大小写均可，解析后统一显示
为小写。`export -i` 为兼容完整导出仍接受 4 到 40 位前缀。

`-i` 不支持用连字符表示范围。每个前缀只在 task 域中查找。没有匹配
时失败；匹配多条时列出完整候选 ID 并失败。20 位仍可能匹配多条，不能
把它当成完整的 40 位 TaskRecordID。

输入中重复指向同一条记录时，只处理一次，并保留第一次出现的位置。
任何 ID 无效时，整条命令在开始输出前失败。

### 3.3 过滤条件

过滤条件统一写为：

```text
-f '<key><op><value>'
```

命令先检查所有过滤条件的名称、运算符和值。只有全部合法后才开始读取
和筛选数据。这样不会因为前一个条件恰好得到空结果而漏掉后续条件中的
拼写错误。

多个 `-f` 按命令行顺序依次处理，含义是所有条件都必须满足。某一步
得到空结果后，不再继续读取后续筛选所需的数据。

`ls` 和 `export` 允许的 key 不同。`get-result` 不接受 `-f`。使用错误
的 key 时，错误信息必须指出该 key 适用于哪条子命令。

### 3.4 输出与日志

终端表格、提示、警告和错误使用现有 `tpblog` 函数，因此同时写入终端
和运行日志。

RIDMAP 和 CSV 文件直接写磁盘，不把文件正文复制到运行日志。

### 3.5 错误码

新命令继续使用 `TPB_MOD_CLI_MISC`，不增加新的模块编号。`main` 仍通过
`tpb_err_to_exit_status` 把错误原因转换为进程退出状态。

## 4. RIDMAP

### 4.1 文件位置

```text
<workspace>/.tmp/tpb_rt_local_ridmap
```

若 `.tmp` 不存在，`task ls` 创建它。

### 4.2 文件格式

文件中每个有效行只有一个 40 位小写十六进制 TaskRecordID：

```text
a1b2c3d4e5f60123456789abcdef0123456789ab
0123abcdef0123456789abcdef0123456789abcd
```

`rid 0` 对应第一个有效行，`rid 1` 对应第二个有效行。读取时跳过空行和
首个非空字符为 `#` 的行。rid 按有效行计数，不按文件的物理行号计数。

写入时不写序号和注释。

### 4.3 更新规则

- `task ls` 实际显示至少一条记录时，整文件替换。
- 先写同目录临时文件，写完并关闭成功后再一次替换旧文件。
- 临时文件和最终文件只允许当前用户读写。
- 显示零条记录时保留旧文件，并明确提示 RIDMAP 没有更新。
- 不自动删除 RIDMAP。
- 多个 `task ls` 同时执行时，最后完成替换的命令生效。

保留旧文件意味着 `export --from-ls` 可能读取更早一次查询的结果。命令
必须在开始时打印 RIDMAP 路径。用户若需要当前筛选结果，应确认刚才的
`task ls` 至少显示了一条记录。

RIDMAP 不记录 workspace 名称。读取 ID 后还要在当前 workspace 的 task
域中确认记录存在。找不到时提示重新执行 `task ls`。

## 5. 初始记录、入口与成员

### 5.1 逻辑任务

用户通过 `-i` 可以直接指定成员记录。成员的 `derive_to` 指向 capsule
或其它归并后的入口。`get-result` 的比较单位是逻辑任务，而不是用户
碰巧指定的某一条物理记录：

- 独立入口对应一个逻辑任务。
- capsule 及其 `TaskID` 列表中的成员共同对应一个逻辑任务。
- 直接指定 capsule 成员时，先解析到入口，再按同一逻辑任务处理。

沿 `derive_to` 走的方向是从当前记录走向目标入口，不是寻找子记录。
这个定义保证 `-r` 选择入口和 `-i` 选择其中一个成员时得到相同的默认
聚合结果。

### 5.2 `get-result` 的自动解析

`get-result` 不接受 `--trace-to-entry`、`--keep-current` 或成员过滤，也
不进行交互询问。每个初始 ID 按以下顺序处理：

1. `derive_to == 0` 时把当前记录作为工作根。
2. 否则自动沿 `derive_to` 走到入口，最多允许 8 跳。
3. 工作根是独立入口时，只读取该记录。
4. 工作根是 capsule 时，读取全部成员；默认聚合，指定
   `--show-each-subrank` 时逐成员显示。

遇到重复 ID、目标文件缺失、目标不在 task 域、断链或超过 8 跳时，该
初始选择失败。所有初始选择必须在打印报告前完成解析。同一工作根由多个
输入指向时只处理一次，并保留第一次出现的位置。

从 capsule 展开的成员不再跟随 `derive_to`，否则会回到同一个 capsule
并造成循环。成员没有指回 capsule 时按 2.4 节给出警告，但仍以 capsule
列表为成员关系依据。

### 5.3 `export` 的追踪选项

`export` 继续支持：

```text
--trace-to-entry
--keep-current
```

两者互斥。

- `--trace-to-entry` 自动沿 `derive_to` 走到 `derive_to == 0` 的入口。
- `--keep-current` 只处理用户直接指定的当前记录。
- 两者都未写且标准输入是交互式终端时，每个直接指定的成员询问一次。
- 两者都未写且标准输入不是交互式终端时，采用 `--keep-current`，并写
  一行说明。

询问文字为：

```text
Trace task <当前ID> to entry <下一条ID>? [y/N]
```

选择 `y` 后不再逐跳询问，而是自动走到入口。最多允许 8 跳。遇到重复
ID、目标文件缺失、目标不在 task 域或超过 8 跳时失败。

### 5.4 `export` 的展开与成员过滤

`export` 的每个初始 ID 最终得到一个工作根：

- 工作根是独立入口时，只处理该记录。
- 工作根是 capsule 时，读取 `TaskID` 列表，再应用 `subrank` 和
  `subtid` 过滤。
- 用户选择保留一个成员时，只处理该成员。

从 capsule 展开的成员不再检查和询问 `derive_to`。否则成员会沿
`derive_to` 回到同一个 capsule，再次展开，造成循环。

`export` 支持：

```text
-f 'subrank=1-10'
-f 'subrank=1,4,5'
-f 'subtid=12ab,30cd'
```

规则如下：

- 只支持 `=`。
- `subrank` 是非负十进制编号，支持逗号列表和闭区间。
- `subtid` 匹配成员完整记录中的 `tid` 字段。
- `tid` 是无符号 32 位整数，按 8 位十六进制字符串匹配开头。
- `subtid` 的筛选值必须是 4 到 8 位十六进制。
- `subtid` 不是 TaskRecordID，也不是 MPI rank。
- 一个 `subtid` 前缀匹配多个成员时，列出候选的 subrank、tid 和 task ID
  后失败。
- 独立任务或保留的单个成员不能使用成员过滤，使用时返回参数错误。

未写成员过滤时，capsule 默认选择全部成员。

## 6. `tpbcli task ls`

### 6.1 语法

```text
tpbcli task ls|list
        [-n N | -N N]
        [-f|--filter '<key><op><value>']...
```

`-n` 和 `-N` 不能同时出现。

- `-n N` 按新到旧显示前 N 条。
- `-N N` 按旧到新显示前 N 条。
- `N == 0` 表示不限制条数。
- 未写 `-n` 或 `-N` 时等同于 `-n 0`。

这里的 `0` 与 `tpbcli db list` 不同。`db list` 默认显示 20 条，`task ls`
默认显示全部符合条件的入口记录。

### 6.2 处理顺序

1. 读取 task 索引。
2. 只保留 `derive_to` 全零的入口行。
3. 记录入口总数。
4. 按 `utc_bits` 排序。
5. 依次应用所有过滤条件。
6. 记录过滤后的条数。
7. 应用 `-n` 或 `-N` 的数量限制。
8. 按最终显示顺序分配连续 rid。
9. 读取 capsule 完整记录以计算 `Subproc`。
10. 打印表格。
11. 显示条数大于零时更新 RIDMAP。

`utc_bits` 相同时，保持 task 索引中的先后顺序。

rid 只属于最近一次成功写入 RIDMAP 的 `task ls` 结果。它与
`tpbcli db list -dt` 的行号没有关系，因为后者按索引追加顺序显示。

### 6.3 表格

```text
Rid  Start Time (Local)  Kernel  Exit  Duration(s)  Handle  Subproc  Task ID  TBatch ID
```

各列规则：

- `Rid` 从 0 连续编号。
- `Start Time` 通过 `tpbcli_task_time_format_local` 显示本机时区时间。
- `Kernel` 用 `kernel_id` 在 kernel 索引中查找名称。
- 找不到 kernel 名称时显示 kernel ID 的 6 位前缀加 `*`，与
  `tpbcli db list` 一致。
- `Duration(s)` 为纳秒除以 `1e9`，保留三位小数。
- `Subproc` 对独立任务显示 `0`，对 capsule 显示成员数，无法读取时显示
  `?`。
- Task ID 和 TBatch ID 显示 6 位前缀加 `*`。

摘要格式：

```text
Found <过滤后条数> in <入口总数> task records, shown <显示条数> results.
```

### 6.4 `ls` 过滤条件

支持以下条件：

| Key | 运算符 | Value | 规则 |
|---|---|---|---|
| `kernel_id` | `=` | 4 到 40 位 hex | 匹配 ID 开头 |
| `kernel_name` | `=` | 内核名 | 区分大小写，完整匹配 |
| `tbatch_id` | `=` | 4 到 40 位 hex | 匹配 ID 开头 |
| `datetime` | `=` `>` `>=` `<` `<=` | 日期时间 | `>` 表示开始得更晚 |
| `exit_code` | `=` | 非负十进制整数 | 完整匹配 |

`kernel_name` 查不到对应 kernel 记录时，该 task 不符合名称条件。

`duration`、`subrank` 和 `subtid` 不属于 `ls` 过滤条件。传入这些名称时
返回参数错误。

运算符先识别 `>=` 和 `<=`，再识别 `=`、`>`、`<`。

### 6.5 日期时间

日期和时间主体固定为：

```text
YYYY-MM-DDTHH:MM:SS
```

允许以下时区写法：

```text
2026-07-21T12:30:00Z
2026-07-21T20:30:00+08:00
2026-07-21T20:30:00+8
2026-07-21T20:30:00+08
2026-07-21T20:30:00+0800
2026-07-21T20:30:00
```

解析规则：

- `Z` 表示 UTC。
- 带正负偏移时，先换算成 UTC。
- 不带时区时，按执行命令的本机时区解释。
- 本机时间换算设置 `tm_isdst = -1`，让系统按该日期判断夏令时。
- 夏令时切换造成同一墙钟时间出现两次时，采用本机 C 库的选择。
- 不存在的本机墙钟时间和无效日历日期直接报错，不接受自动改成其它时间。
- 需要跨机器得到一致结果时应写 `Z` 或明确时区。
- 当前可表示的年份为 1970 到 2225。
- 最终比较值必须重新编码成时区部分为零的 UTC `utc_bits`，不能把带
  时区的墙钟字段直接与 task 记录比较。

## 7. `tpbcli task get-result`

### 7.1 语法与默认选择

```text
tpbcli task get-result|gr
        (-r|--rid LIST | -i|--task-id HASH_LIST)
        [--data-name NAME_LIST]...
        [--meta-name META_LIST]...
        [--show-each-subrank]
```

`-r` 与 `-i` 必须且只能选择一个。

名称选项的默认规则如下：

- 两个名称选项都未出现时，显示默认 meta 列和所有 shared data name。
- 只出现 `--data-name` 时，显示默认 meta 列和指定 data。
- 只出现 `--meta-name` 时（未传入 `--data-name`），显示指定 meta，不输出
  Record Data。
- 两者都出现时，分别显示指定的 meta 和 data。
- `--data-name` 参数为空字符串 `''` 时，等价于明确不选择任何 data：不输出
  Record Data。可单独使用（配合默认 meta），也可与 `--meta-name` 组合。

`--meta-name` 的名称列表为空时参数错误。`--data-name` 允许整段参数为空
字符串（见上）；CSV 列表内的空条目（例如 `a,,b`）仍是参数错误。

### 7.2 名称列表语法

`NAME_LIST` 在一个命令行参数内使用 CSV 风格语法：

- 逗号只在双引号外分隔条目。
- 未引用条目的首尾 ASCII 空白会被忽略。
- 双引号必须包住整个条目；引号内的逗号属于名称。
- 引号内用两个连续双引号表示一个双引号字符。
- 空条目、未闭合引号和引号外的非空尾随字符均为参数错误。
- 名称区分大小写，必须完整匹配。
- 重复名称只保留第一次出现的位置。
- 同一选项可重复，效果等同于按出现顺序连接各列表。

例如：

```bash
--data-name 'a,b,c'
--data-name '"test,x", "test,y"'
--data-name '"a""b",c'
```

第二例选择 `test,x` 和 `test,y`；第三例第一个名称是 `a"b`。这里采用
CSV 的双引号规则，不把反斜线定义为转义符。

`--data-name --help`、`--data-name -h`、`--meta-name --help` 和
`--meta-name -h` 是上下文名称查询。它们仍要求 `-r` 或 `-i`，解析所选
记录并打印 7.3 节的名称报告后成功退出，不打印结果表。若实际 header
名称恰好是 `--help` 或 `-h`，必须让 CSV 双引号成为参数内容，例如
`--data-name '"--help"'`，以消除语法歧义。现有 argp 不支持
`--data-name=VALUE`，设计不能依赖等号形式。普通的 `get-result --help`
只打印静态命令帮助，不读取 rafdb。

### 7.3 Shared 与 Private 名称

`--data-name` 只在每条普通 task 的输出范围
`[ninput, ninput + noutput)` 内按 `header.name` 查找。输入参数、capsule
的 `TaskID` 链接和末尾环境 header 不属于 Record Data。这个边界保证
固定统计列只处理输出指标；需要逐元素内容时使用 `task export`。

名称报告先把每个输入解析为 5.1 节定义的逻辑任务，再输出：

```text
Shared meta name: ref, taskid, kernel, batch_host, datetime, nmember, ...
Shared data name: Copy, Scale, Add, Triad
Private meta name (ref=r1): header[12].name, header[12].tag
Private data name (ref=r1): RankLocalMetric
```

定义如下：

- Shared meta name 是所有所选逻辑任务都能解析的 meta key 交集。
- Shared data name 是所有所选逻辑任务都存在且 schema 兼容的 data name
  交集。
- Private 名称是某个逻辑任务的可用名称减去对应 Shared 集合。
- 输出名称按第一条逻辑任务中的顺序排列；Private 名称按本任务中的顺序
  排列。
- 某个名称在 capsule 中只存在于部分成员时，不进入 Shared data name。
  它显示在该逻辑任务的 Private 行，并追加成员范围，例如
  `RankLocalMetric[subrank=0,2-3]`。
- 只有一个逻辑任务时，满足 capsule 内全成员一致条件的名称属于 Shared；
  Private 仍可揭示只存在于部分成员的名称。

名称报告中的 `ref` 使用输入引用，不总是 rid：通过 `-r` 选择时为 `r0`、
`r1`；通过 `-i` 选择时按解析后且去重的输入顺序为 `i0`、`i1`。不能把
`i0` 写成 rid，因为 RIDMAP 中未必存在该记录。

### 7.4 Meta Data

默认 Meta Data 列为：

```text
Ref TaskID Kernel BatchHost Datetime NMember ExitCode NInput NOutput NHeader
```

各默认列含义：

- `Ref` 是 7.3 节的输入引用。
- `TaskID` 是解析后工作根的 6 位小写前缀加 `*`。
- `Kernel` 从 `kernel_id` 查找；失败时显示 kernel ID 的 6 位前缀加 `*`。
- `BatchHost` 来自关联 tbatch 的 `hostname`。它是批次前端主机，不保证
  是每个成员实际执行的主机；tbatch 缺失时显示 `N/A` 并警告。
- `Datetime` 使用工作根的 UTC 时间。对 capsule，工作根是 capsule 入口
  记录本身，因此显示 capsule 记录的 `utc_bits`，不在成员间做 consensus，
  也不因成员时间不一致显示 `mixed`。独立任务的工作根即该任务自身。
- `NMember` 对独立任务为 1，对 capsule 为成员 ID 数量。它不是 `nproc`，
  因为 subrank 不保证等于 MPI rank。
- `ExitCode`、`NInput`、`NOutput`、`NHeader` 对独立任务取本记录值；对
  capsule 取成员的一致值，不一致时显示 `mixed`。不能使用 capsule 自身
  固定的 `0`、`0`、`0`、`1` 代替成员值。

当前 task 的 `duration` 固定写为 0，因此不放入默认列。它仍可通过
`--meta-name duration` 或 `duration_seconds` 显式查看。

`--meta-name` 支持以下派生 key：

```text
ref
taskid
kernel
batch_host
nmember
```

支持以下 `task_attr_t` key：

```text
task_record_id
derive_to
inherit_from
tbatch_id
kernel_id
datetime
utc_bits
btime
duration
duration_seconds
exit_code
handle_index
pid
tid
ninput
noutput
nheader
reserve
```

还支持 `header[INDEX].FIELD`，其中 `INDEX` 是非负十进制数，`FIELD` 为：

```text
block_size
ndim
data_size
type_bits
type_bits.source
type_bits.check
type_bits.type
_reserve
uattr_bits
uattr_bits.cast
uattr_bits.shape
uattr_bits.trim
uattr_bits.kind
uattr_bits.name
uattr_bits.base
uattr_bits.value
name
tag
note
dimsizes
dimnames
```

header 下标只适合低层诊断。同一个下标在不同内核或不同版本中可能指向
不同名称，不能把 `header[2].uattr_bits` 当成跨任务的稳定指标选择器。
data 选择必须使用 `--data-name`。

ID key 显示完整 40 位小写十六进制，`taskid` 例外，按默认短格式显示。
`datetime` 显示 ISO UTC，`duration` 为纳秒整数，`duration_seconds` 为秒。
位字段的子字段显示符号名，无法识别的位同时保留原始十六进制值。

默认聚合模式下，成员字段和 header 字段均采用 consensus 规则：所有可读
成员值相同才显示该值，否则显示 `mixed`。`task_record_id`、`derive_to`、
`pid`、`tid` 等天然逐成员变化的字段通常会显示 `mixed`。例外：`datetime`、
`utc_bits` 和 `btime` 在默认聚合下取工作根（capsule 入口或独立任务）的
值，与默认 `Datetime` 列一致。指定 `--show-each-subrank` 后，Meta Data
增加 `Subrank` 列并逐成员显示原值；工作根自身不再额外产生一行，此时
上述时间字段也按成员各自取值。只有部分成员可读时 consensus 只基于可读
成员，并必须给出 `used <可用数>/<总成员数> members` 警告。

### 7.5 Data 匹配与聚合

默认情况下，对每个逻辑任务和每个 data name 合并所有可读成员的原始
数值样本，再对合并后的单一数组计算统计量。不能先对每个成员算 mean
再计算 mean；这种 “mean of means” 会在成员样本数不同时改变权重。

可统计的数据类型为：

```text
TPB_INT_T
TPB_INT8_T
TPB_INT16_T
TPB_INT32_T
TPB_INT64_T
TPB_UINT8_T
TPB_UINT16_T
TPB_UINT32_T
TPB_UINT64_T
TPB_FLOAT_T
TPB_DOUBLE_T
```

各成员样本转换成 `double` 后合并。不同数值类型可以合并，`Type` 列显示
`mixed`；非数值类型不能进入统计。统计转换与现有 `tpb_stat_*` 一样可能
无法精确表示大于 `2^53` 的 64 位整数，前端不声称结果仍是整数精确值。

同名 header 只有同时满足以下条件才可聚合：

- 每个参与成员中恰好匹配一个输出 header。
- 数据类型在上述支持列表中。
- unit 与 shape 位一致；前端不静默换算不同单位。
- 逻辑元素数和 `data_size` 一致，乘法计算无溢出。
- 数组非空。

成员之间的数组长度可以不同，合并后 `Dim` 显示总样本数。多维数组按
连续存储顺序展平，并对该 ref/name 给出一次警告；`--show-each-subrank`
模式下 `Dim` 显示每个成员自己的维度。

同一成员中有多个同名输出 header 时无法只靠名称消除歧义。该
ref/name 显示 `N/A` 并警告，不任意选择第一个 header。若部分成员缺失、
损坏或 schema 不兼容，默认聚合其余可用成员，同时明确警告
`used <可用数>/<总成员数> members`；没有可用成员时显示 `N/A`。

指定 `--show-each-subrank` 时不跨成员合并。独立任务的 `Subrank` 显示
`-`；capsule 每个成员各产生一行，缺失成员也保留一行 `N/A`，以免行号
错位。规范名称使用 subrank 而不是 rank，因为 2.4 节已明确它不保证等于
MPI rank。可以把 `--show-each-rank` 留作未来兼容别名，但帮助和设计不得
把它描述成真实 MPI rank。

### 7.6 固定统计列

Record Data 固定显示：

```text
mean min max p25 p50 p75 p90 p95 p99
```

不提供 reducer 参数。mean、min、max 调用现有 `tpb_stat_*`；分位数调用
`tpb_stat_qtile_1d`。分位函数先排序，再取 `floor(q * n)` 位置的元素，
越过末尾时限制到最后一个元素，不在相邻元素间插值。

数值格式沿用 `tpb_cliout_results` 的有效数字、trim 和 unit 显示规则，
而不是旧设计中固定输出 15 位小数。shape 为 point 且逻辑任务只有一个
样本时只填 mean，其余列为 `-`。capsule 的 point 指标由每个成员贡献一个
样本；合并后样本数大于 1 时显示完整的跨成员统计。非 point 的单元素
数组也按现有 run 行为只填 mean。

### 7.7 输出布局

各段之间使用现有 `tpblog` 水平分隔线。以下示例假定显式请求了
`--data-name 'Triad,Add'`：

```text
Selected tasks: r0(a1b2c3*), r1(d4e5f6*)
--------------------------------------------------------------------------------
Meta Data
Ref TaskID  Kernel BatchHost Datetime             NMember ExitCode NInput NOutput NHeader
r0  a1b2c3* stream n01       2026-07-22T01:02:03Z 4       0        3      4       10
r1  d4e5f6* stream n02       2026-07-22T02:03:04Z 1       0        3      4       10
--------------------------------------------------------------------------------
Record Data
Ref Name  Type   Unit          Dim mean min max p25 p50 p75 p90 p95 p99
r0  Triad double TPB_UNIT_MBPS 400 ...
r1  Triad double TPB_UNIT_MBPS 100 ...
--------------------------------------------------------------------------------
Warnings
WARNING: Data name Add does not exist in ref=r1 (task=d4e5f6*).
--------------------------------------------------------------------------------
Note: capsule member results were aggregated; use --show-each-subrank to display each member.
```

`--show-each-subrank` 时两个表都在 `Ref` 后增加 `Subrank`。未传入
`--data-name`，或 `--data-name` 参数为空字符串 `''` 时，省略 Record Data、
Warnings 中的 data 警告和聚合 Note。没有警告时省略 Warnings 段。没有
capsule 发生聚合时省略 Note。

Record Data 先按用户请求的 data name 顺序，再按输入 Ref 顺序，最后按
subrank 排序。这样同一指标的各任务相邻，便于横向比较。名称报告、表格、
提示、警告和错误均通过 `tpblog` 双写到终端与日志；该命令不写报告文件。

### 7.8 缺失数据与退出状态

名称、ID 列表和 meta key 的语法错误在任何报告输出前失败。记录读取后的
运行时问题按以下规则处理：

- 某个 ref/name 缺失、重复、损坏或不兼容时显示 `N/A`，在 Warnings 段
  给出准确原因，其它结果继续。
- capsule 的部分成员无法读取时保留可用结果并警告；全部成员无法读取时
  该逻辑任务不可用。
- 显式请求的 meta key 在某个逻辑任务中不存在时显示 `N/A` 并警告。
- 未请求 Record Data（未传入 `--data-name` 或 `--data-name ''`）且至少
  一个逻辑任务可读时允许成功。
- 请求 data 时，只要至少一个 ref/name 成功产生统计行，命令成功并保留
  警告；全部 data 均不可统计时返回 `TPBE_METRIC_MISSING`。
- 初始 ID 无效、自动追踪断链、capsule 结构损坏或所有逻辑任务均不可读
  时返回非零状态。

这里的表格是人类可读输出，不承诺稳定的机器解析格式。需要逐元素数据
或稳定文件边界时使用 `task export`。

## 8. `tpbcli task export`

### 8.1 语法

```text
tpbcli task export
        (-r|--rid LIST | -i|--id LIST | --from-ls)
        [--trace-to-entry | --keep-current]
        [-f|--filter 'subrank=...' | -f|--filter 'subtid=...']...
        [-o|--outdir DIR]
```

`-r`、`-i` 和 `--from-ls` 必须且只能选择一个。

`--from-ls` 读取 RIDMAP 中的全部有效行。文件不存在或没有有效 ID 时
失败。

### 8.2 输出目录

默认输出根目录：

```text
<workspace>/csv/
```

`-o` 指定其它根目录。每个工作根建立一个子目录：

```text
<outdir>/<工作根的40位ID>/
```

工作根可能是独立任务、保留的单个成员或 capsule。

若目录已经存在且含有文件，则依次尝试：

```text
<40位ID>_1
<40位ID>_2
<40位ID>_3
```

若目录不存在则创建。若目录存在且为空则使用该目录。

每个文件先写同目录临时文件，关闭成功后再改成最终文件名。某个文件
失败时删除它的临时文件。已经完成的其它文件保留，命令列出完成和失败
的文件并返回非零状态。

### 8.3 文件名

capsule 使用：

```text
capsule_<capsuleID>_meta.csv
capsule_<capsuleID>_data.csv
```

独立任务和成员使用关联 tbatch 的 hostname：

```text
<hostname>_meta_<taskID>.csv
<hostname>_data_<taskID>.csv
```

hostname 的来源和处理规则：

1. 用 task 的 `tbatch_id` 读取 tbatch 记录。
2. 使用 tbatch 的 `hostname`。
3. 文件名只保留英文字母、数字、点、下划线和连字符，其它字节改成
   下划线。
4. tbatch 缺失、hostname 为空或读取失败时使用 `localhost` 并给出警告。

这个 hostname 是批次前端主机名，不保证是该成员的实际执行主机名。

### 8.4 导出哪些记录

独立任务或保留的单个成员：

- 写该记录的 meta 文件。
- 写该记录的 data 文件。

capsule：

- 总是写 capsule 自身的 meta 和 data 文件。
- 默认再写全部成员各自的 meta 和 data 文件。
- 有成员过滤时，只写选中成员的文件。

成员过滤不会改写 capsule 自身的 data 文件。capsule data 仍列出它在
磁盘中保存的全部成员 ID，因为该文件描述的是完整 capsule 记录。

capsule 中同一成员 ID 出现多次时，`get-result --show-each-subrank` 保留
每个 subrank 行，默认聚合也按每个位置贡献一次样本，并给出重复 ID
警告。`export` 只写一组成员文件，同样给出警告。

### 8.5 本功能所称的 CSV

本功能按已确定的要求使用连续两个逗号 `,,` 分隔字段，文件扩展名仍为
`.csv`。

这不是通用 CSV 规则。普通 CSV 阅读器若只支持单逗号和双引号，不能
保证正确读取。读取程序必须明确把 `,,` 当作一个完整分隔符。

这种格式会改写少数字符串，不能逐字节恢复原内容。需要无损导出时应
在以后增加其它格式，不能依靠本版文件反向重建 `.tpbr`。

### 8.6 单元格处理

写入任意非空字符串前，按以下顺序处理：

1. 反斜线 `\` 写成 `\\`。
2. 回车写成可见文本 `\r`。
3. 换行写成可见文本 `\n`。
4. 字符串内部连续两个或更多逗号时，在每对相邻逗号之间插入空格。
5. 字符串以逗号结尾时，在末尾补一个空格。

示例：

```text
some,,content  写成 some, ,content
some,          写成 some, 
```

空单元格保持为空，不做上述处理。

单个 `TPB_CHAR_T` 元素的值恰好是逗号时，单元格写入一个原始 `0x1C`
字节。其它不可打印的单字节字符写成 `0xNN`。`TPB_INT8_T`、
`TPB_UINT8_T`、`TPB_SIGNED_CHAR_T` 和 `TPB_UNSIGNED_CHAR_T` 默认按整数
处理，带 `TPBLINK` 的 ID 数据除外。

读取程序按 `,,` 从左到右分列，不对插入的空格或 `0x1C` 做反向恢复。

### 8.7 meta 文件

meta 文件有两列，第一行固定为：

```text
field,,value
```

task 固定属性至少包含：

```text
task_record_id
derive_to
inherit_from
tbatch_id
kernel_id
utc_bits
start_utc
btime
duration
exit_code
handle_index
pid
tid
ninput
noutput
nheader
reserve
record_datasize
```

随后按 header 下标写入：

```text
header[0].block_size
header[0].ndim
header[0].data_size
header[0].type_bits
header[0].type_bits.source
header[0].type_bits.check
header[0].type_bits.type
header[0]._reserve
header[0].uattr_bits
header[0].uattr_bits.cast
header[0].uattr_bits.shape
header[0].uattr_bits.trim
header[0].uattr_bits.kind
header[0].uattr_bits.name
header[0].uattr_bits.base
header[0].uattr_bits.value
header[0].name
header[0].tag
header[0].note
header[0].dimsizes
header[0].dimnames
```

`type_bits` 和 `uattr_bits` 原始值必须保留为十六进制。拆出的各部分使用
现有 `db dump` 的符号名称。未知值写成 `UNKNOWN_` 加十六进制，不丢弃
原始位。这样以后增加新值时，旧导出工具仍保留完整信息。

`dimsizes` 和 `dimnames` 写出全部 7 个位置。数组内部用单个逗号分开，
文件列之间仍使用两个逗号。

磁盘固定区中另有 128 字节保留区。公开 task 读取函数不返回这 128
字节，因此 meta 文件不导出它。这里所说的全部固定属性，是
`task_attr_t` 公开提供的字段，不是 `.tpbr` 固定区的逐字节副本。

位字段名称表应由 `task export` 与 `db dump` 共用。实现时把现有
`tpbcli-database-dump-fmt.c` 中的解码代码移到 tpbcli 共用文件，或提供
共用函数。不得在两个目录中各维护一份名称表。

### 8.8 data 文件的列

第一行是列名。普通情况下，每个 header 对应一列，列顺序与
`headers[]` 相同，列名只写 `header.name`。

不同 header 可以使用相同名称。导出时不改名，读取程序必须按列位置和
meta 文件中的 header 下标区分。

各列数据行数可以不同。文件总数据行数取最长列的行数，较短列在后续行
留空。

三个字段且中间一列为空时：

```text
value0,,,,value2
```

其中四个连续逗号表示两个分隔符，中间是一个空字段。

### 8.9 数值与其它普通类型

数值格式不受本机语言设置影响，小数点始终是 `.`。

- `float` 使用小数点后 7 位。
- `double` 使用小数点后 15 位。
- 有符号整数使用十进制。
- 无符号整数使用十进制，不先转换成有符号数。
- `nan`、`inf` 和 `-inf` 使用这些小写文本。
- 当前公开元素大小函数不支持的类型，按一个逻辑元素的原始字节写成
  `0x` 开头的十六进制。

普通数值一行写一个逻辑元素。多维数组按连续存储顺序展开，
`dimsizes[0]` 变化最快。

普通 `TPB_STRING_T`：

- 一维时整段字符串写入第一行的一个单元格。
- 多维时，每个外层位置写一行，每格最多读取 `dimsizes[0]` 字节。
- 遇到第一个 NUL 字节结束该格文本。

### 8.10 环境变量列

当完整检测到三条环境变量 header，且类型和长度都合法时：

- 不输出三条原始列。
- 在第一条环境 header 原来的位置插入两列。
- 两列名为 `environment_variable_key` 和
  `environment_variable_value`。
- 每行写一个 key 和按 count 恢复后的 value。
- `environment_variable_count` 不出现在 data 文件中。
- meta 文件仍保留三条原始 header 的说明。

三条 header 不完整时，不启用特殊处理，按普通 header 导出。

三条 header 完整但内容互相矛盾时，该记录的 data 导出失败，不猜测
剩余 value 属于哪个 key。

这种处理便于阅读，但 data 文件不再与原 header 一列对一列，也不能
重建原始环境变量数据块。这是本版明确接受的限制。

### 8.11 capsule data

capsule data 文件通常只有一列：

```text
TaskID
```

每行写一个 40 位小写十六进制成员 ID，顺序就是 subrank 顺序。

该列按 `TPBLINK`、`TaskID` 名称和每块 20 字节共同识别，不能只根据
`TPB_UNSIGNED_CHAR_T` 判断。

## 9. 与现有命令的关系

### 9.1 与 `tpbcli db`

- `db list -dt` 按 task 索引追加顺序显示。
- `task ls` 按开始时间显示。
- `db dump -dt -i ID` 只显示指定的那一条记录。
- `task get-result` 和 `task export` 可以从 capsule 展开成员。
- `db dump -e` 使用单逗号的简单文本。
- `task export` 使用双逗号分隔的独立 meta 和 data 文件。

两组命令不能共用行号，也不能把一种输出当成另一种输出解析。

### 9.2 与 benchmark

`task get-result` 复用与 benchmark 相同的公开统计函数和输出名称精确
匹配规则，但行为有意不同：

- `get-result` 会展开 capsule 成员；当前 benchmark 路径跳过成员。
- `get-result` 把成员原始样本合并后统计；benchmark 对每条记录先应用
  reducer，再对多个标量求平均。
- `get-result` 的统计列固定，不接受 reducer，也不存在未知 reducer
  回退到 mean 的行为。
- `get-result` 对重复名称和 schema 不兼容给出 `N/A` 与警告，不任意取
  第一个 header。

因此 MPI 任务在 `get-result` 中得到的聚合结果不应直接假设与 benchmark
得分输入相同。

## 10. 读取失败与不完整数据

### 10.1 `ls`

- task 索引读取失败时整条命令失败。
- kernel 索引读取失败时仍可列 task，Kernel 列显示 kernel ID 前缀并
  给出一次警告。
- capsule 完整记录读取失败时仍列入口，Subproc 显示 `?`。
- RIDMAP 只写实际显示且有有效 TaskRecordID 的行。

### 10.2 `get-result`

- 初始 ID、自动解析目标或 capsule 记录缺失时失败。
- 某个成员文件缺失时显示 `N/A` 或使用其余成员聚合，并明确给出警告。
- 全部成员都无法读取时该逻辑任务不可用；所有逻辑任务不可用时失败。
- 部分 ref/name 没有指标时允许完成；请求的 data 全部不可统计时返回
  `TPBE_METRIC_MISSING`。
- meta-only 查询至少有一个逻辑任务可读时允许成功。

### 10.3 `export`

- 每个决定导出的 `.tpbr` 都必须有一份 meta 和一份 data。
- 同一记录只完成了一份文件时，该记录视为失败。
- 命令结束时列出成功记录数、失败记录数和输出目录。
- 任一记录失败时返回非零状态，但保留已经成功完成的文件。

## 11. 实现文件

建议在 `src/tpbcli/task/` 中按职责分文件：

```text
tpbcli-task.h
tpbcli-task.c
tpbcli-task-select.c
tpbcli-task-ls.c
tpbcli-task-get-result.c
tpbcli-task-export.c
tpbcli-task-csv.c
```

各文件用途：

- `tpbcli-task.c` 建立参数树并分派子命令。
- `tpbcli-task-select.c` 处理 RIDMAP、ID 前缀、自动解析、export 追踪、
  capsule 和成员筛选。
- `tpbcli-task-ls.c` 处理索引筛选、排序、表格和 RIDMAP 写入。
- `tpbcli-task-get-result.c` 处理名称列表、上下文名称报告、成员聚合、
  meta/record 表和警告。
- `tpbcli-task-export.c` 处理目录、文件名、meta 和 data 内容。
- `tpbcli-task-csv.c` 只处理双逗号文件的单元格和行写入。

顶层还需修改：

- `src/tpbcli/main/tpbcli.c` 注册 `task`。
- 构建文件加入上述源文件。
- 顶层帮助加入 `task`。

位字段名称解码应放在 tpbcli 共用位置，让 `db dump` 和 `task export`
共同调用。rafdb 数据读取仍只走公开函数。

## 12. 测试要求

测试放在 `tests/` 中并沿用现有 tpbcli 测试写法。具体测试编号在实施前
由维护者指定。

至少覆盖：

### 12.1 `ls`

- 只显示 `derive_to == 0` 的入口。
- 相同 `utc_bits` 保持索引顺序。
- `-n 0`、`-N 0` 和正数限制。
- 每种过滤条件和多个过滤条件。
- 日期时间的 UTC、明确偏移和本机时区。
- 夏令时不存在时间报错。
- capsule 成员数。
- kernel 记录缺失。
- 零结果不更新 RIDMAP。
- RIDMAP 临时文件替换。

### 12.2 ID、自动解析与 export 追踪

- rid 列表和闭区间。
- `get-result` 的 6 到 20 位 ID 前缀和 `export` 的 4 到 40 位前缀。
- 无匹配和多个匹配。
- 重复 ID 只处理一次。
- `get-result -i` 指定成员时自动解析到与 `-r` 相同的入口结果。
- `get-result` 的断链、重复 ID 和超过 8 跳。
- 交互回答 `y` 与默认 `N`。
- 非交互输入默认保留当前记录。
- `export --trace-to-entry` 与 `--keep-current` 冲突。
- 两条命令从 capsule 展开后都不再次追踪成员。

### 12.3 `get-result`

- `get-result` 与 `gr` 两个命令名。
- `--data-name` 和 `--meta-name` 的单项、列表、重复选项和去重顺序。
- 引号内逗号、成对双引号、空项、未闭合引号和双引号 `--help` 消歧。
- 无名称选项、data-only、meta-only 和两个名称选项同时出现。
- 上下文帮助的 Shared/Private 交集、差集和部分 subrank 标记。
- 输出范围内区分大小写地精确匹配指标。
- 指标缺失、同名 header、空数组、损坏长度和不支持的类型。
- 独立任务、capsule 原始样本合并和禁止 mean of means。
- 不同成员样本数、混合数值类型、单位/shape 不兼容和多维展平。
- point 单样本和 capsule 多成员 point 统计。
- 默认 meta、成员 consensus、`mixed` 和 `header[index].field`。
- BatchHost 与 NMember 的准确语义。
- 默认聚合与 `--show-each-subrank` 的行顺序、缺失成员占位。
- 分位数位置与现有统计函数一致。
- 部分 ref/name 警告后成功、全部 data 不可统计和 meta-only 成功。

### 12.4 `export`

- 每条记录同时生成 meta 和 data。
- capsule 自身与选中成员。
- 成员过滤不裁剪 capsule data。
- tbatch hostname、文件名替换和 `localhost`。
- 已存在目录的数字后缀。
- 双逗号、空字段和四个连续逗号。
- 字符串中的连续逗号、尾逗号、反斜线和换行。
- 单字符逗号写成 `0x1C`。
- float 7 位和 double 15 位。
- 有符号与无符号整数。
- 多维数组和多维字符串。
- 环境三列变成 key/value 两列。
- 空环境快照的 4 字节占位值。
- 未知 type 和 unit 位仍保留原始十六进制。
- 文件写入一半失败时不留下临时文件。

## 13. 文档更新

功能实现时必须同步更新：

- `docs/USAGE.md`
- 相关中文使用文档
- 顶层命令帮助
- MPI 结果查看说明

使用文档要明确说明：

- `task ls` 默认显示全部，`db list` 默认显示 20 条。
- rid 只来自最近一次写入 RIDMAP 的 `task ls`。
- `get-result` 会自动解析并展开 capsule；`export` 可按选项展开，
  `db dump` 不会展开。
- `get-result` 默认合并成员原始样本，subrank 不保证等于 MPI rank。
- BatchHost 来自 tbatch 前端，不保证是成员实际运行主机。
- 双逗号文件不是普通单逗号 CSV。
- 环境变量编码不能完整保留空的冒号段。
- 文件名中的 hostname 来自 tbatch，不一定是成员实际运行主机。

`tpbcli task`（`ls` / `get-result|gr` / `export`）已实现；用户文档见
[`docs/USAGE.md`](../USAGE.md) §2.6 与 [`docs/USAGE_CN.md`](../USAGE_CN.md) §2.6，
CLI help 与 Pack **B7** 测试随实现同步维护。

## 14. 使用示例

```bash
# 查找最近的 stream 成功任务，并刷新 RIDMAP
tpbcli task ls -n 20 \
    -f 'kernel_name=stream' \
    -f 'exit_code=0'

# 显示 RIDMAP 中前两条入口的默认 meta 和全部 shared output
tpbcli task get-result -r 0,1

# 对比指定指标；引号内逗号属于名称
tpbcli task gr -r 0,1 --data-name 'Triad,"latency,p99"'

# 只显示指定 meta；成员 ID 会自动解析到入口
tpbcli task gr -i a1b2c3 --meta-name \
    'task_record_id,kernel,batch_host,nmember,exit_code'

# 列出所选逻辑任务的 shared/private 名称
tpbcli task gr -r 0,1 --data-name --help

# 不聚合 capsule，逐 subrank 显示
tpbcli task gr -r 0 --data-name 'Copy,Scale,Add,Triad' \
    --show-each-subrank

# 从指定成员追踪到入口，再选择部分成员
tpbcli task export -i a1b2c3d4 \
    --trace-to-entry \
    -f 'subrank=0-3' \
    -o /tmp/out

# 导出 RIDMAP 中全部入口
tpbcli task export --from-ls
```
