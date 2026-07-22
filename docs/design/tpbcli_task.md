# `tpbcli task` 前端设计

## 1. 目标

`tpbcli task` 用来查找、对比和导出 task 记录。它新增三个子命令：

```text
tpbcli [--workspace PATH] task ls
tpbcli [--workspace PATH] task extract
tpbcli [--workspace PATH] task export
```

三条命令分别解决以下问题：

1. `ls` 按条件查找任务，按开始时间排序，为本次结果分配短编号。
2. `extract` 从若干任务中读取同一个指标或同一个固定属性，在终端中对比。
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
- 用户通过 `extract -o` 指定的文本文件
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
为 `?` 并给出警告。`extract` 和 `export` 在需要展开它时失败，不猜测成员。

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

`extract` 和 `export` 展示的是按当前编码能够恢复的值，不得宣称它与
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

`extract` 和 `export` 支持以下两种 ID 来源：

- `-r` 或 `--rid` 读取 RIDMAP 中的短编号。
- `-i` 或 `--id` 读取 task ID 的十六进制前缀。

两者不能同时出现。

`-r` 支持：

```text
0
0,1,3
0-3
0,2-4,8
```

范围是闭区间。短编号必须是非负十进制整数。

`-i` 支持用逗号分开的 4 到 40 位十六进制前缀：

```text
a1b2
a1b2c3,01234567
```

`-i` 不支持用连字符表示范围。每个前缀只在 task 域中查找。没有匹配
时失败；匹配多条时列出完整候选 ID 并失败。

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

`ls` 和 `extract`、`export` 允许的 key 不同。使用错误的 key 时，错误
信息必须指出该 key 适用于哪条子命令。

### 3.4 输出与日志

终端表格、提示、警告和错误使用现有 `tpblog` 函数，因此同时写入终端
和运行日志。

RIDMAP、`extract -o` 文件和 CSV 文件直接写磁盘，不把文件正文复制到
运行日志。

`extract -o FILE` 只把完整报告写入 `FILE`。终端和日志只写一行完成
消息及文件路径。

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

### 5.1 为什么需要追踪

用户通过 `-i` 可以直接指定成员记录。成员的 `derive_to` 指向 capsule
或其它归并后的入口。用户可能只想处理当前成员，也可能想从入口展开整
组成员。

沿 `derive_to` 走的方向是从当前记录走向目标入口，不是寻找子记录。

### 5.2 追踪选项

`extract` 和 `export` 都支持：

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

### 5.3 展开规则

每个初始 ID 最终得到一个工作根：

- 工作根是独立入口时，只处理该记录。
- 工作根是 capsule 时，读取 `TaskID` 列表，再应用 `subrank` 和
  `subtid` 过滤。
- 用户选择保留一个成员时，只处理该成员。

从 capsule 展开的成员不再检查和询问 `derive_to`。否则成员会沿
`derive_to` 回到同一个 capsule，再次展开，造成循环。

### 5.4 成员过滤

`extract` 和 `export` 支持：

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
Rid  Start Time (UTC)  Kernel  Exit  Duration(s)  Handle  Subproc  Task ID  TBatch ID
```

各列规则：

- `Rid` 从 0 连续编号。
- `Start Time` 通过 `tpb_ts_bits_to_isoutc` 显示。
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

## 7. `tpbcli task extract`

### 7.1 语法

```text
tpbcli task extract
        (-r|--rid LIST | -i|--id LIST)
        (-m|--metric NAME | -F|--field FIELD)
        [--reducer NAME]
        [--trace-to-entry | --keep-current]
        [-f|--filter 'subrank=...' | -f|--filter 'subtid=...']...
        [--show-env]
        [-o|--out FILE]
```

`-r` 与 `-i` 必须且只能选择一个。`-m` 与 `-F` 也必须且只能选择一个。

`--reducer` 只允许与 `-m` 一起使用。`-F` 同时带 `--reducer` 时返回参数
错误。

### 7.2 表格中的记录

独立任务产生一行，`Subrank` 显示 `-`。

capsule 本身不产生指标行。它的固定属性显示在该组的简要信息中，成员
各产生一行。这样不会把 capsule 的 `TaskID` 链接误当成业务指标。

表格使用 `Ref` 标识用户输入来源：

- 通过 rid 选择时显示 `r0`、`r1`。
- 通过 ID 选择时显示解析后的 task ID 6 位前缀加 `*`。

### 7.3 指标模式

`-m NAME` 只在输出范围
`[ninput, ninput + noutput)` 中按 `header.name` 完整匹配，区分大小写。

一条记录中没有该指标时显示 `N/A`。同名输出 header 多于一个时，该
记录有歧义，命令失败。全部记录都没有该指标时，命令返回
`TPBE_METRIC_MISSING`。

统计只支持以下数据类型：

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

空数组、数据长度错误和其它类型不能计算统计量。若只有部分记录不能
计算，则这些行显示 `N/A` 并给出原因；若全部记录都不能计算，则命令
失败。

支持的统计名称：

```text
mean
min
max
median
p50
pXX
Q=<值>
```

默认是 `mean`。`median` 与 `p50` 相同。`pXX` 中的值按百分数解释。
`Q=` 后的值大于 1 时按百分数解释，否则按 0 到 1 的比例解释。最终值
必须落在 0 到 1 之间。

未知名称、尾部多余字符和超出范围的值都报错，不沿用 benchmark 中把
未知名称改成 `mean` 的现有行为。

分位数计算调用现有 `tpb_stat_qtile_1d`。它先排序，再取
`floor(q * n)` 位置的元素，并把越过末尾的位置限制到最后一个元素。
它不在相邻元素之间取平均。

指标表：

```text
Metric: Triad    Reducer: mean
Ref      Subrank  Task ID   Kernel  Exit  Unit         Value
r0       0        a1b2c3*   stream  0     TPB_UNIT_MBPS 12345.670000000000000
r0       1        d4e5f6*   stream  0     TPB_UNIT_MBPS 12001.200000000000000
998877*  -        998877*   stream  0     TPB_UNIT_MBPS 11000.000000000000000
```

统计结果以 `double` 计算并保留小数点后 15 位。表中保留单位列，因为
不同记录中的同名指标可能使用不同单位。前端不自动换算单位。

### 7.4 固定属性模式

`-F FIELD` 支持以下名称：

```text
task_record_id
derive_to
inherit_from
tbatch_id
kernel_id
kernel_name
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

字段格式：

- `datetime` 显示 ISO UTC。
- `duration` 显示原始纳秒整数。
- `duration_seconds` 显示秒，保留三位小数。
- ID 字段显示完整 40 位十六进制。
- `kernel_name` 从 kernel 索引查找，找不到时显示 kernel ID 的 6 位前缀。
- 其它整数使用十进制。

属性表仍按 `Ref` 和 `Subrank` 排列。

### 7.5 环境变量显示

默认不打印环境变量，以免大量进程环境遮住对比表。指定 `--show-env`
后，在每条实际任务记录下显示：

```text
Environment for r0 subrank 0
  PATH = /usr/bin:/bin
  FOO = v0:v1
```

若三条环境 header 不完整、类型不符或 count 与 key 数量不符，该记录
显示环境解析错误。指标和属性仍可继续显示，命令最后给出警告。

### 7.6 页脚

报告末尾固定写：

```text
Hint: use `tpbcli task export ...` to write raw arrays as CSV.
```

这里的 raw arrays 指指标等普通 header 的逐元素内容。环境变量会按
key/value 形式整理，不能视为逐字节原始数据。

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

capsule 中同一成员 ID 出现多次时，`extract` 保留每个 subrank 行；
`export` 只写一组成员文件，并给出重复 ID 警告。

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
- `task extract` 和 `task export` 可以从 capsule 展开成员。
- `db dump -e` 使用单逗号的简单文本。
- `task export` 使用双逗号分隔的独立 meta 和 data 文件。

两组命令不能共用行号，也不能把一种输出当成另一种输出解析。

### 9.2 与 benchmark

`task extract` 复用与 benchmark 相同的统计函数和指标名称匹配规则，但
有两处有意不同：

- `task extract` 会展开 capsule 成员。
- `task extract` 遇到未知统计名称时失败，不自动改成 `mean`。

因此 MPI 任务在 `task extract` 中可以从成员得到指标，不代表当前
benchmark 路径也会以相同方式展开 capsule。

## 10. 读取失败与不完整数据

### 10.1 `ls`

- task 索引读取失败时整条命令失败。
- kernel 索引读取失败时仍可列 task，Kernel 列显示 kernel ID 前缀并
  给出一次警告。
- capsule 完整记录读取失败时仍列入口，Subproc 显示 `?`。
- RIDMAP 只写实际显示且有有效 TaskRecordID 的行。

### 10.2 `extract`

- 初始 ID、追踪目标或 capsule 记录缺失时失败。
- 某个成员文件缺失时，该行显示 `N/A` 并给出警告；其它成员继续。
- 全部成员都无法读取时失败。
- 部分记录没有指标时允许完成，全部没有时失败。

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
tpbcli-task-extract.c
tpbcli-task-export.c
tpbcli-task-csv.c
```

各文件用途：

- `tpbcli-task.c` 建立参数树并分派子命令。
- `tpbcli-task-select.c` 处理 RIDMAP、ID 前缀、追踪、capsule 和成员筛选。
- `tpbcli-task-ls.c` 处理索引筛选、排序、表格和 RIDMAP 写入。
- `tpbcli-task-extract.c` 处理指标统计、属性表和环境显示。
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

### 12.2 ID 与追踪

- rid 列表和闭区间。
- 4 到 40 位 ID 前缀。
- 无匹配和多个匹配。
- 重复 ID 只处理一次。
- 交互回答 `y` 与默认 `N`。
- 非交互输入默认保留当前记录。
- `--trace-to-entry` 与 `--keep-current` 冲突。
- 断链、重复 ID 和超过 8 跳。
- capsule 展开后不再次追踪成员。

### 12.3 `extract`

- 输出范围内精确匹配指标。
- 指标缺失、空数组和不支持的类型。
- 每种统计名称。
- 非法统计名称不改成 `mean`。
- 分位数位置与现有统计函数一致。
- 独立任务与 capsule 成员表。
- `subrank` 和 `subtid`。
- 环境变量空值和连续冒号造成的已知信息丢失。

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
- `task` 会展开 capsule，`db dump` 不会。
- 双逗号文件不是普通单逗号 CSV。
- 环境变量编码不能完整保留空的冒号段。
- 文件名中的 hostname 来自 tbatch，不一定是成员实际运行主机。

## 14. 使用示例

```bash
# 查找最近的 stream 成功任务，并刷新 RIDMAP
tpbcli task ls -n 20 \
    -f 'kernel_name=stream' \
    -f 'exit_code=0'

# 对比 RIDMAP 中前两条入口的 Triad 指标
tpbcli task extract -r 0,1 -m Triad --reducer mean

# 显示环境变量
tpbcli task extract -r 0 -m Triad --show-env

# 直接保留指定成员，不追踪到 capsule
tpbcli task extract -i a1b2c3d4 -m Triad --keep-current

# 从指定成员追踪到入口，再选择部分成员
tpbcli task export -i a1b2c3d4 \
    --trace-to-entry \
    -f 'subrank=0-3' \
    -o /tmp/out

# 导出 RIDMAP 中全部入口
tpbcli task export --from-ls
```
