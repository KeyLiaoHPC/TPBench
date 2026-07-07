# Runtime Environment Domain 设计

## 1. 背景与目标

`runtime_environment` domain 用于记录一次 TPBench 运行依赖的软件栈和
环境变量集合。它补足 `kernel` domain 的编译历史：`kernel` 记录描述被测
kernel 如何构建，`runtime_environment` 记录运行前 shell 中显式选择了哪些
应用、库和环境变量。

该设计借鉴 Environment Modules 的工作方式：用户创建一个可命名、可继承
的环境记录，再通过 `tpbcli rtenv load` 将其中的变量导出到当前 shell。
TPBench 后续生成的 task 与 tbatch 计数会回写到当前激活的环境记录，便于
按软件环境追踪测试结果。

本特性初始范围：

- 在 rafdb 中新增 `runtime_environment` domain。
- 新增 `tpbcli rtenv <new|list|show|load>` 前端（`new` 亦可写作 `create`）。
- 记录用户显式声明的应用程序/库三元组、环境变量键值与加载模式。
- 强制“始终存在一个激活 RTEnv”：编译安装的 post-build 脚本基于编译期环境
  创建 `id == 0` 的基础环境；此后 `tpbcli rtenv new` 必须继承当前激活 RTEnv。
- 在 task/tbatch 记录中预留指向 RTEnv `id` 的外键，实现结果→环境的可靠反查。

非目标：

- 不实现完整 modulefile 解释器。
- 不自动扫描或采集整个 shell 环境；只记录用户显式声明的内容。
- 不校验 application 的实际版本或解析路径；用户对自己的实验环境负责。
- 不在 RTEnv 中记录 OMP/NUMA 等运行时变量；这些由 `tpbcli run` 在运行时作为
  task 的“来自环境变量的输入参数”记录（见 §7）。
- 不在 `tpbcli load` 进程中直接修改父 shell；加载通过输出 shell 片段完成。

## 2. 术语

**Runtime Environment (RTEnv)** 是一条可复用的运行环境记录。每条记录有
唯一 `name` 和域内顺序 `id`。`id` 是从 0 开始的 `int32`，只在当前工作区的
`runtime_environment` domain 内保证唯一。

**Application** 是用户显式记录的应用程序或库，例如 `gcc`、`openmpi`、
`libnuma`。每个 application 由 `[app_name, version_str, note]` 三个 64 字节
字符串组成。

**Variable** 是用户显式记录的环境变量操作，包含 `key`、`value` 与 `mode`。
`mode` 为 32 位无符号整数：

| 值 | 语义 |
|----|------|
| `0` | override：覆盖同名变量 |
| `1` | prepend：将值前置到同名变量 |
| `2` | append：将值追加到同名变量 |
| `3` | unset：清除同名变量；`value` 记录为空串 |

`prepend`/`append` 默认使用 `:` 作为分隔符，适用于 `PATH`、
`LD_LIBRARY_PATH` 等 POSIX 路径变量。未来可增加 per-variable 分隔符字段；
初始版本不记录。

**Base Environment（基础环境）** 是 `id == 0` 的 RTEnv，由编译安装的
post-build 脚本基于编译期环境创建，代表“TPBench 自身构建时的软件栈”。它是
所有后续 RTEnv 继承链的根；post-build 需保证只创建一次（幂等）。

## 3. rafdb 布局

目录结构：

```text
${TPB_WORKSPACE}/rafdb/runtime_environment/
├── runtime_environment.tpbe
├── 0000000000.tpbr
├── 0000000001.tpbr
└── ...
```

`runtime_environment` 使用域内数值 ID，而不是现有 tbatch/kernel/task 的
20 字节 SHA-1 ID。因此：

- `.tpbe` 中的 `id` 是 `int32`。
- `.tpbr` 文件名为 10 位零填充十进制 ID，后缀 `.tpbr`。
- `tpbcli db dump --id` 的全局 SHA-1 查找不匹配 RTEnv；RTEnv 由
  `tpbcli rtenv show -i <id>` 或后续 `db dump --rtenv-id <id>` 访问。
- rafdb L1 domain registry 需要允许该 domain 使用 numeric-id path builder。

新增 magic domain nibble：

| Domain | LO |
|--------|----|
| `task_batch` | `0x0` |
| `kernel` | `0x1` |
| `task` | `0x2` |
| `runtime_environment` | `0x3` |

## 4. 属性模型

### 4.1 C 结构建议

```c
#define TPB_RAF_RTENV_NAME_LEN     256
#define TPB_RAF_RTENV_HOST_LEN     256
#define TPB_RAF_RTENV_NOTE_LEN     256
#define TPB_RAF_RTENV_RESERVE_LEN  1024
#define TPB_RAF_RTENV_APP_STR_LEN  64

typedef struct tpb_raf_rtenv_attr {
    int32_t      id;                      /**< Domain-local numeric ID */
    char         name[256];               /**< Unique environment name */
    char         hostname[256];           /**< Hostname where created */
    tpb_dtbits_t utc_bits;                /**< Creation time */
    int32_t      inherit_from;            /**< Parent environment ID or -1 */
    int32_t      derive_to;               /**< Derived environment ID or -1 */
    uint32_t     ntask;                   /**< Task records generated under it */
    uint32_t     ntbatch;                 /**< TBatch records generated under it */
    char         note[256];               /**< Human-readable description */
    uint32_t     napp;                    /**< Application/library count */
    uint32_t     nenv;                    /**< Environment variable count */
    uint32_t     nheader;                 /**< Record header count, tpbr only */
    unsigned char reserve[1024];          /**< Reserved, zero-filled */
} tpb_raf_rtenv_attr_t;
```

`inherit_from` 和 `derive_to` 使用 `-1` 表示无链接。子环境的 `inherit_from`
指向父环境 ID（强制继承下总是指向创建时的激活 RTEnv）；`derive_to` 记录父环境
**最近一次**直接派生的子环境 ID，仅作快捷提示。由于强制继承会形成一父多子，
**完整的派生关系由 L3 link header（`TPBLINK::DeriveTo`）记录**（见 §6.1 / §8），
而非单个 `derive_to` 字段。

### 4.2 `.tpbe` 字段

`.tpbe` 存储索引字段，按追加顺序写入。`name` 必须在该工作区内唯一。

| 字段 | 类型/大小 | 说明 |
|------|-----------|------|
| `id` | `int32` | 从 0 开始的顺序编号 |
| `name` | `char[256]` | 环境名，唯一 |
| `hostname` | `char[256]` | 创建主机名 |
| `utc_bits` | `uint64` | 创建时间 |
| `inherit_from` | `int32` | 父环境 ID，或 `-1` |
| `derive_to` | `int32` | 衍生环境 ID，或 `-1` |
| `ntask` | `uint32` | 基于当前环境生成的 task 数 |
| `ntbatch` | `uint32` | 基于当前环境生成的 tbatch 数 |
| `note` | `char[256]` | 环境描述 |
| `napp` | `uint32` | 应用/库数量 |
| `nenv` | `uint32` | 环境变量数量 |
| `reserve` | `uint8[1024]` | 保留，写零 |

计数规则：

- 生成 task record 时，无论 kernel 返回值如何，当前激活环境的 `ntask += 1`。
- 生成 tbatch record 时，当前激活环境的 `ntbatch += 1`。
- 没有激活环境时不回写计数。

### 4.3 `.tpbr` 字段

`.tpbr` 的元数据部分包含 `.tpbe` 字段的超集，并额外记录 `nheader`：

| 字段 | 类型/大小 | 说明 |
|------|-----------|------|
| `id` | `int32` | 与 `.tpbe` 一致 |
| `name` | `char[256]` | 与 `.tpbe` 一致 |
| `hostname` | `char[256]` | 与 `.tpbe` 一致 |
| `utc_bits` | `uint64` | 与 `.tpbe` 一致 |
| `inherit_from` | `int32` | 与 `.tpbe` 一致 |
| `derive_to` | `int32` | 与 `.tpbe` 一致 |
| `ntask` | `uint32` | 与 `.tpbe` 一致 |
| `ntbatch` | `uint32` | 与 `.tpbe` 一致 |
| `note` | `char[256]` | 与 `.tpbe` 一致 |
| `napp` | `uint32` | 显式记录的应用和库数量 |
| `nenv` | `uint32` | 环境变量数量 |
| `nheader` | `uint32` | meta header 数量 |
| `reserve` | `uint8[1024]` | 保留，写零 |

固定 header 顺序：

1. `application`
2. 对每个环境变量 `i`，依次写入 `key[i]`、`value[i]`、`mode[i]`

因此：

```text
nheader = 1 + nenv * 3
```

当 `napp == 0` 时仍写入 `application` header，`data_size = 0`。

在增量继承语义下（见 §6.1），`napp` / `nenv` 只统计当前环境相对父环境的增量
条目；完整环境内容需沿 `inherit_from` 链合并得到。

### 4.4 Record Data

`application` header：

- 名称：`application`
- 类型：`TPB_STRING_T`
- 维度：`ndim = 3`
- `dimsizes = {64, 3, napp}`
- 逻辑布局：`application[napp][3][64]`
- 三列分别是 `app_name`、`version_str`、`note`

每个变量写三条 header：

| Header 名称 | 类型 | 维度 | 数据 |
|-------------|------|------|------|
| `key[i]` | `TPB_STRING_T` | 1D | `strlen(key) + 1` 字节，含 `\0` |
| `value[i]` | `TPB_STRING_T` | 1D | `strlen(value) + 1` 字节，含 `\0` |
| `mode[i]` | `TPB_UINT32_T` | 1D | 单个 `uint32_t` |

`value[i]` 的长度必须按 `strlen(value) + 1` 计算；如果实现中沿用原草案的
`len(key) + 1`，会截断 value，本文档将其修正为 value 的真实长度。

## 5. 激活状态

TPBench 从本特性起**强制要求始终存在一个激活 RTEnv**。判定“当前激活 RTEnv”
时，**只信任 shell 环境变量 `$TPB_RTENV_ID`**（由 `tpbcli rtenv load` 导出）。
工作区 `etc/config.json` 只保存“基础环境 ID”（默认 `0`），用作 `$TPB_RTENV_ID`
缺失时的回退，而不是运行期激活状态的真相来源：

```json
{
  "name": "default",
  "runtime_environment": {
    "base_id": 0
  }
}
```

激活解析顺序：

1. 若 `$TPB_RTENV_ID` 已设置且指向存在的 RTEnv，使用它。
2. 否则回退到 `base_id`（默认 `0`），并打印 warning，提示用户通过
   `tpbcli rtenv load` 显式加载，避免结果被误归属到基础环境。
3. 若连基础环境都不存在（例如未执行 post-build），`tpbcli run` / `benchmark`
   报错并提示先创建/加载 RTEnv。

`$TPB_RTENV_ID` 只表示某个 shell 声明加载了哪个 RTEnv；它不证明 shell 里其余
变量确实与该 RTEnv 一致。设计上 RTEnv 在 kernel 运行期间**不被修改**，因此
记录侧只保存声明值并在 task/tbatch 中预留指向该 `id` 的外键（见 §7），不在运行时
对实际环境做全量快照。

`tpbcli rtenv list` 显示：

```text
Activated runtime environment ID: 0
```

无法解析时（`$TPB_RTENV_ID` 未设置且无基础环境）显示：

```text
Activated runtime environment ID: N/A
```

让变量进入当前 shell 需执行 `eval "$(tpbcli rtenv load -i <id>)"` 或
`source <(tpbcli rtenv load -i <id>)`。

## 6. CLI 设计

顶层新增：

```text
tpbcli rtenv <new|list|show|load>
```

建议别名：

- `rtenv`
- `runtime-environment`

### 6.1 `tpbcli rtenv new`

命令形式：

```bash
tpbcli rtenv new \
  [--name <environment_name>] [--note <note>] [--inherit-from <id|name>] \
  [[-o|--output-file <file_path>] | [-f|--input-file <file_path>]] \
  [--add-application --name <app_name> --version <app_version> --note <note>]... \
  [--variable --name <key> --value <value>]... \
  [--prepend-variable --name <key> --value <value>]... \
  [--append-variable --name <key> --value <value>]... \
  [--unset-variable --name <key>]...
```

`--name` 在顶层表示环境名；在 `--add-application`、`--variable`、
`--prepend-variable`、`--append-variable`、`--unset-variable` 后表示当前条目的
名称。实现时应使用 `tpbcli-argp` 的上下文栈解析，避免把二者混淆。若实现复杂度
过高，可在首版额外支持无歧义别名 `--env-name`、`--app-name` 和 `--var-name`。

`tpbcli rtenv new`（别名 `create`）**必须继承当前激活 RTEnv**：`inherit_from`
默认取当前激活 RTEnv 的 `id`；`--inherit-from` 可显式指定其它父环境。若无法解析
激活 RTEnv（且未指定 `--inherit-from`），命令报错。这保证每条 RTEnv 都能沿
`inherit_from` 回溯到基础环境（`id == 0`）。

**继承语义（增量）：** 子环境只存储相对父环境的**增量**——新增或覆盖的
applications、变量，以及 `mode=3` 的 unset。子记录的 `napp` / `nenv` 只统计该
增量条目，不含父环境内容。因此：

- `tpbcli rtenv show` / `load` 时，从基础环境（`id == 0`）沿 `inherit_from` 链
  自顶向下依次合并到目标环境：同名变量子覆盖父，`mode=3` 从合并结果中删除该
  变量，applications 按 `app_name` 去重后子覆盖父。
- 合并在 `load` 生成 shell 片段前完成；生成顺序为最终合并结果的稳定顺序
  （父在前、子在后，子内按记录顺序）。
- 继承链存在环（`inherit_from` 指回自身或形成回路）时报错。

创建子环境时，除写入子 `.tpbe` / `.tpbr` 外，还向父环境的 L3 派生 link
（`TPBLINK::DeriveTo`）追加子 `id`，并把父 `derive_to` 更新为该子 id（持域锁）。

模式：

- 无 `-o` / `-f` 且无 application/variable 选项：打开编辑器编辑模板。
- `-o <file>`：输出模板到文件，不修改 rafdb。
- `-f <file>`：解析文件并写入 rafdb。
- 命令行直接给出 application/variable：直接写入 rafdb。

`-o` 与 `-f` 互斥。`-o` 不允许与直接写入选项混用，因为它只生成模板。

编辑器选择顺序：

1. `$VISUAL`
2. `$EDITOR`
3. `nano`
4. `vi`

编辑流程类似 `visudo`：

1. 创建临时文件并写入模板。
2. 调用编辑器。
3. 编辑器正常退出后解析文件。
4. 解析失败则打印错误和临时文件路径，不写 rafdb。
5. 解析成功后追加 `.tpbe` 和 `.tpbr`，并删除临时文件。

模板格式为一行一个 CLI 片段，`#` 开头为注释：

```text
# Required unless --name was passed on command line.
--name 'env_name'
--note 'notes_of_the_environment'
# inherit_from defaults to the currently active RTEnv; override if needed.
# --inherit-from '0'

--add-application --name 'app_name' --version 'app_version' --note 'notes_of_the_app'
--variable --name 'environment_variable_name' --value 'environment_variable_value'
--prepend-variable --name 'PATH' --value '/path/to/bin'
--append-variable --name 'LD_LIBRARY_PATH' --value '/path/to/lib'
--unset-variable --name 'SOME_VAR_TO_CLEAR'
```

解析规则：

- 使用与 CLI 一致的 shell-like tokenizer，支持单引号和双引号。
- `--value=='foo'` 视为用户输入错误；正确格式为 `--value 'foo'` 或
  `--value='foo'`。
- 环境名、应用名和变量名不能为空。
- 环境 `name` 必须唯一。
- 同一个环境中允许多条同名变量操作，按记录顺序加载。
- application 的每个字符串字段超过 63 字节时报错，不静默截断。

ID 分配：

- 读取 `runtime_environment.tpbe` 中最大 ID。
- 新 ID 为 `max_id + 1`；空域从 `0` 开始。
- 写入时使用 domain 文件锁，避免并发 `new` 分配重复 ID。

### 6.2 `tpbcli rtenv list`

输出当前激活 ID，然后列出 `.tpbe` 内容：

```text
Activated runtime environment ID: 0

ID  Name        Hostname    Created UTC           NTask  NTBatch  NApp  NEnv  Note
0   gcc-openmpi node01      2026-07-07T10:00:00Z      3        1     2     4  GCC 13 + OpenMPI
```

列宽使用 `tpb_log_printf_c` / `tpblog_printf_c` 的比例控制，并在列内换行。
过长单词在列宽内用连字符断开。

建议列比例：

| 列 | 比例 |
|----|------|
| ID | 6 |
| Name | 18 |
| Hostname | 14 |
| Created UTC | 20 |
| NTask | 8 |
| NTBatch | 8 |
| NApp | 6 |
| NEnv | 6 |
| Note | 剩余 |

### 6.3 `tpbcli rtenv show`

命令形式：

```bash
tpbcli rtenv show [-i <runtime_environment_id> | --name <environment_name>]
```

不指定选择器时显示当前激活环境；若无激活环境则报错并提示使用
`tpbcli rtenv list`。

输出内容：

- 固定属性：ID、name、hostname、created UTC、inherit/derive、计数、note。
- Applications 表。
- Environment variables 表，包含 key、mode、value。
- 可执行的加载提示：`eval "$(tpbcli rtenv load -i <id>)"`。

### 6.4 `tpbcli rtenv load`

命令形式：

```bash
tpbcli rtenv load [-i <runtime_environment_id> | --name <environment_name>]
```

不指定选择器时加载当前激活环境（按 §5 顺序解析）。`load` 将目标环境的变量与
`TPB_RTENV_ID` 作为 shell 片段输出到 stdout；运行期激活状态由该导出的环境变量
承载，不写回 `config.json`。日志和诊断必须输出到 stderr，避免污染 `eval`。

示例：

```bash
eval "$(tpbcli rtenv load -i 0)"
```

输出的 shell 片段示例：

```bash
export TPB_RTENV_ID='0'
export TPB_RTENV_NAME='gcc-openmpi'
if [ -n "${PATH:-}" ]; then export PATH='/opt/gcc/bin':"$PATH"; else export PATH='/opt/gcc/bin'; fi
if [ -n "${LD_LIBRARY_PATH:-}" ]; then export LD_LIBRARY_PATH="$LD_LIBRARY_PATH":'/opt/gcc/lib64'; else export LD_LIBRARY_PATH='/opt/gcc/lib64'; fi
export OMP_NUM_THREADS='16'
```

加载规则：

- `mode=0`：`export KEY='value'`
- `mode=1`：若 `KEY` 非空则前置为 `value:$KEY`，否则设置为 `value`
- `mode=2`：若 `KEY` 非空则追加为 `$KEY:value`，否则设置为 `value`
- `mode=3`：`unset KEY`
- value 需要 shell 单引号转义（含内嵌单引号）。
- key 必须匹配 `[A-Za-z_][A-Za-z0-9_]*`，否则拒绝生成 shell。
- 变量按 `.tpbr` 中的记录顺序输出。
- `load` 始终 `export TPB_RTENV_ID` / `TPB_RTENV_NAME`；运行期激活状态以此
  环境变量为准，`load` 不再把它写回 `config.json`（`config.json` 仅存基础环境
  `base_id`，见 §5）。

## 7. 与 run/benchmark 的集成

`tpbcli run` 和 `tpbcli benchmark` 在创建 tbatch 前按 §5 解析当前激活 RTEnv ID
（只信任 `$TPB_RTENV_ID`，缺失时回退基础环境）。

**外键（本特性新增）：** 在 `tbatch_attr` 与 `task_attr` 中新增 `int32
runtime_environment_id` 字段，记录该 tbatch/task 运行时的激活 RTEnv `id`。这提供
结果→环境的可靠反查，不再仅依赖 RTEnv 侧的有损计数。

> 实现方式：该字段从既有 reserve 区划出 4 字节，`.tpbe` / `.tpbr` 总大小不变，
> 无需 bump 记录版本或迁移旧工作区。旧记录该区为零，读为 `0`，对应基础环境
> （`id == 0`），语义上等价于“在默认 / 构建时环境下运行”。

**计数：**

- 创建 tbatch record 成功后，激活 RTEnv 的 `ntbatch += 1`。
- 每生成一条 task record 后，`ntask += 1`（无论 kernel 返回值）。
- 计数为 read-modify-write，共享 / NFS 工作区并发时须持域文件锁，避免丢失
  increment。计数是聚合视图；单条结果的反查以外键为准。

**运行时变量（`--kenvs`）：** RTEnv 只作为 `tpbcli run` 启动时的基础环境；它
**不记录** OMP / NUMA 等运行时变量。`--kenvs` 传入的 per-handle 环境变量在运行时
**总是覆盖**同名变量，并由 task 记录为“来自环境变量的输入参数”（header
`type_bits` 的 source 段标记为环境来源），与来自 `--kargs` 的输入参数并列。
因此单条 task 记录同时保留：RTEnv 外键（基础环境）+ 实际生效的 `--kenvs` 值。

## 8. 实现计划

### 8.1 rafdb

新增文件：

| 文件 | 作用 |
|------|------|
| `src/corelib/rafdb/rafdb-l2-runtime-environment.c` | RTEnv entry/record 读写、ID 分配 |
| `src/corelib/rafdb/rafdb-l3-runtime-environment-counter.c` | `ntask`/`ntbatch` 计数 patch |
| `src/corelib/rafdb/rafdb-l3-runtime-environment-derive.c` | 派生关系 L3 link header（`TPBLINK::DeriveTo`，一父多子）读写/追加 |
| `src/corelib/rafdb/tpb-raf-runtime-environment.h` | domain 常量与内部声明 |

需要修改：

- `rafdb-l1-domain-reg.c/h`：注册 `runtime_environment` 目录、entry 名、
  magic domain 和 numeric-id 文件命名。
- `rafdb-l1-magic.c`：接受 domain nibble `0x3`。
- `rafdb-l1-workspace.c`：初始化工作区时创建
  `rafdb/runtime_environment/`。
- `rafdb-l2-task.c` / `rafdb-l2-tbatch.c`：`task_attr` / `tbatch_attr` 新增
  `runtime_environment_id`（从 reserve 划出 4B，entry / tpbr 总大小不变，无需
  bump 版本）；旧记录该区为零，读为 `0` = 基础环境。
- task 写入路径（`tpb-autorecord.c` / 驱动）：解析激活 RTEnv 并回填外键与计数；
  把 `--kenvs` 作为环境来源的输入参数 header 写入 task。
- `src/include/tpb-public.h`：公开 RTEnv 类型和函数。

建议公共 API：

```c
int tpb_raf_entry_append_rtenv(const char *workspace,
                               const tpb_raf_rtenv_attr_t *entry);
int tpb_raf_entry_list_rtenv(const char *workspace,
                             tpb_raf_rtenv_attr_t **entries,
                             size_t *count);
int tpb_raf_record_write_rtenv(const char *workspace,
                               const tpb_raf_rtenv_attr_t *attr,
                               const tpb_meta_header_t *headers,
                               const void *data,
                               uint64_t datasize);
int tpb_raf_record_read_rtenv(const char *workspace,
                              int32_t id,
                              tpb_raf_rtenv_attr_t *attr,
                              tpb_meta_header_t **headers,
                              void **data,
                              uint64_t *datasize);
int tpb_raf_record_patch_rtenv_counters(const char *workspace,
                                        int32_t id,
                                        uint32_t add_ntask,
                                        uint32_t add_ntbatch);
```

### 8.2 CLI

新增目录：

```text
src/tpbcli/rtenv/
├── tpbcli-rtenv.c
├── tpbcli-rtenv-new.c
├── tpbcli-rtenv-list.c
├── tpbcli-rtenv-show.c
├── tpbcli-rtenv-load.c
└── tpbcli-rtenv-template.c
```

顶层 `src/tpbcli/main/tpbcli.c` 注册 `rtenv` 子命令并委托到
`tpbcli_rtenv()`。CLI 只包含 `src/include/tpb-public.h` / `tpbench.h` 和
自身 argp 头，不包含 rafdb 私有头。

### 8.3 配置

新增配置 helper：

- 读取 active ID：缺失返回 `TPBE_LIST_NOT_FOUND`。
- 写入 active ID：原子更新 `etc/config.json`。
- 清理 active ID 可作为后续 `tpbcli rtenv unload` 扩展。

## 9. 测试计划

Corelib 测试：

- 创建空工作区后存在 `rafdb/runtime_environment/`。
- 首条 RTEnv ID 为 0，连续写入 ID 递增。
- 重名 `name` 被拒绝。
- `.tpbe` 单条写入后 list 可读回固定字段。
- `.tpbr` 写入后可读回 application、key/value/mode headers 和 payload。
- `ntask`/`ntbatch` patch 同步更新 `.tpbe` 与 `.tpbr`。
- 增量继承：子环境沿 `inherit_from` 合并父链后，同名变量子覆盖父、`mode=3`
  从合并结果中删除变量、applications 按 `app_name` 去重。
- 创建子环境后父环境的 L3 派生 link 含子 `id`，父 `derive_to` 指向最近子 `id`。
- 继承链成环时报错。

CLI 测试：

- `rtenv new -o <file>` 只生成模板，不新增记录。
- `rtenv new -f <file>` 解析模板并新增记录。
- 命令行 application/variable 可直接新增记录。
- 错误模板不写 rafdb，并报告行号。
- `rtenv list` 在无激活环境时显示 `N/A`。
- `rtenv show -i 0` 显示 application 与变量表。
- `rtenv load -i 0` 只在 stdout 输出 shell 片段，stderr 输出日志。
- `eval "$(tpbcli rtenv load -i 0)"` 后当前 shell 中变量符合 mode 规则
  （可作为 integration 或手工测试）。

Run/benchmark 集成测试：

- 激活 RTEnv 后执行一个成功 kernel，`ntbatch += 1`、`ntask += 1`，且 tbatch /
  task 记录的 `runtime_environment_id` 等于激活 id。
- 激活 RTEnv 后执行一个失败 kernel，只要生成 task record，`ntask += 1`。
- shell 未设置 `$TPB_RTENV_ID` 时回退基础环境 `0` 并打印 warning，外键记为 `0`。
- 无基础环境（未 post-build）时 run / benchmark 报错提示先创建 / 加载 RTEnv。
- `--kenvs` 覆盖同名变量，且作为环境来源输入参数出现在 task 记录中。

## 10. 开放问题

- 是否需要 `tpbcli rtenv unload`（输出 `unset TPB_RTENV_ID` 等）恢复到基础环境。
- `prepend`/`append` 是否需要 per-variable 分隔符以支持非 POSIX path-like 变量。
- 是否需要多主机 / MPI 每节点环境记录（首版不做）。
