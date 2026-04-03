# 数据记录组件设计

## 1. 介绍

### 1.1. 设计原理与架构

TPBench 记录影响 kernel 测试输出的数据，并捕获 kernel 调用产生的结果。由于实际性能对程序上下文和底层环境高度敏感，TPBench 为每条记录标记专用 ID，以便与其他 kernel 和环境因素结合进行进一步分析。

目标指标是计算硬件状态、软件栈配置和工作负载输入参数/数据复杂组合的结果测量。理论上，可以对整个系统进行快照以及目标指标，构建一个涵盖性能、功耗、可移植性、准确性和弹性特性的综合数据库。然而，计算系统中开销和观察粒度之间的权衡是不可避免的。

最终，数据记录频率和技术因数据方面而异。TPBench 使用多个**数据域**来记录计算系统的不同方面，如系统状态、kernel 定义、输入参数、目标指标等。不同记录具有预定义的记录触发条件，并通过 SHA-1 ID 链接。这种方法避免了为每次测试记录完整的快照，同时保持了可追溯性。

每个域具有固定名称记录和动态记录：
- **属性 (Attribute)**: 用于表征域中每个记录实例的静态属性，在指定的字节位置记录，具有预定义的固定名称、定义、格式和解析器。
- **头部 (Header)**: 头部用于定义单维或多维记录数据的名称和用途，包括固定头部和自定义头部。头部是其所包含记录数据的说明部分。
- **记录数据 (Record Data)**: TPBench 记录的数据。

在 **rafdb** 后端中，**属性**、**头部**和**记录数据**存储在 TPBench 条目文件和 TPBench 记录文件中：
- **TPBench 条目文件** (`<DomainName>.tpbe`): 记录工作空间中每个单条记录的属性，以 8 字节 TPBench magic 签名开头作为文件类型和域的标识。数据中的所有属性值以固定数据大小位置存储。每个新记录追加一组属性作为条目到记录文件。
- **TPBench 记录文件** (`<RecordID>.tpbr`): 动态记录数据，由元数据部分和数据部分构成。记录文件中的数据是自解释和自指向的。每个新记录生成一个新的记录文件。

每个域记录表征任务批次和 kernel 调用的状态、输入和输出的系统状态的特定方面。然而，在每次 kernel 调用时记录所有域会导致显著的开销。因此，`${TPB_WORKSPACE}/etc/config.json` 中的设置可以控制记录行为：
- `auto_collect`: 布尔标志，用于启用/禁用域的自动收集
- `action`: 记录触发条件（例如 `"kernel_invoke"`, `"user_invoke"`）

### 1.2. 概念

**任务批次 (Task Batch, TBatch)**: 任务批次 (tbatch) 是由单次 TPBench 前端调用发起的执行上下文（例如 `tpbcli run`）。一个 tbatch 可以执行一个或多个 kernel 调用，每个调用都有自己的输入参数。tbatch 内的所有 kernel 调用共享同一个 TBatchID，该 ID 标记执行期间产生的所有记录。为了获得有效的测试结果，tbatch 需要与实际场景相匹配的稳定运行时环境。具体来说：
- 如果用户使用脚本顺序调用多个 `tpbcli` 命令，每次调用应被视为单独的 tbatch。
- 如果底层硬件或软件配置在调用之间发生变化，每种配置状态构成一个单独的 tbatch。（请不要自找麻烦。）

**TBatchID**: 任务批次的唯一标识符，生成方式如下：
```
SHA1("tbatch" + <utc_bits> + <btime> + <hostname> + <username> + <front_end_pid>)
```
示例：`SHA1("tbatch20250308T130801Z3600000000000node01testuser13249")`

**Kernel**: kernel 是用户希望评估的程序或代码模块。kernel 位于 `${TPB_HOME}/lib` 或 `${TPB_WORKSPACE}/lib/` 目录下，命名为 `tpbk_<kernel_name>.<so|x>`。

---

**工作负载 (Workload)**: 工作负载是用于基准测试的 kernel 的具体实例化，由以下因素定义：
- kernel 名称（标识要执行的代码）
- 输入参数（分配给 kernel 参数定义的值）
- 数据大小（问题规模，例如数组长度、内存大小）

工作负载代表基准测试执行的*可配置*方面。硬件状态、操作系统配置和其他环境因素*不属于*工作负载定义的一部分，而是作为执行上下文单独记录。

---

**参数 (Parameter)**: 参数是由 kernel 定义的命名、类型化的占位符，代表可能影响测量结果的可配置因素。参数具有：
- 名称（例如 `ntest`、`total_memsize`）
- 数据类型（int、double、string 等）
- 默认值
- 可选的验证约束（范围、列表、自定义）

---

**参数值 (Argument)**: 参数值是在工作负载执行期间提供给参数的具体值（数字、字符串或数据引用）。参数值通过 CLI 选项（`--kargs`、`--kenvs`、`--kmpiargs`）或配置文件提供。

---

**目标指标 (Target Metric)**: 目标指标是用户希望从工作负载执行中测量、观察或导出的量。最常见的指标是运行时间。在 TPBench 中，指标还可能包括：
- 性能指标（吞吐量、延迟、FLOPS）
- 正确性、准确性或其他算术精度指标
- 资源使用（功耗、内存带宽）
- 来自工作负载的任何用户定义的观测值

---

**域 / 记录 (Domain / Record)**: 记录存储影响 kernel 测试输出的数据和 kernel 调用产生的结果。每条记录包括唯一 SHA-1 ID 和相关 ID 以链接和被链接。每条记录属于一个域，该域将不同类型的记录分类到不同的方面，并允许在不同场景中获取和记录不同的数据。

## 2. 记录数据架构

### 2.1. 数据库的逻辑组织

TPBench 的记录存储和管理在工作空间下，除了特殊的跨工作空间工具外。TPBench 使用两个环境变量来导航文件系统：`TPB_HOME` 是 TPBench 安装路径，`TPB_WORKSPACE` 是 TPBench 项目的根文件夹。

**1) 工作空间解析：**
1. 如果明确设置了 `${TPB_WORKSPACE}` 环境变量，则使用该目录作为工作空间。
2. 否则，搜索 `$HOME/.tpbench/`。

有关构建系统和文件系统结构的相关信息，请参阅 [design_build.md](./design_build.md)。

---

**2) 文件结构**

TPBench 实现集成的 `rafdb` 后端用于数据管理和操作。未来将添加更多后端（如 SQLite、HDF5）。

在 `rafdb` 中，每个记录域有两种文件：TPBench 头部文件 (.tpbe) 和 TPBench 记录文件 (.tpbr)。`.tpbe` 是追踪当前工作空间中域所有记录的增量头部，`.tpbr` 存储部分属性、头部和详细记录数据。

文件树：
```
${TPB_WORKSPACE}/rafdb/
├── task_batch/
│   ├── task_batch.tpbe 
│   ├── <TBatchID_0>.tpbr
│   ├── <TBatchID_1>.tpbr
│   └── ...
├── kernel/
│   ├── kernel.tpbe
│   ├── <KernelID_0>.tpbr
│   ├── <KernelID_1>.tpbr
│   └── ...
├── task/
│   ├── task.tpbe
│   ├── <TaskRecordID_0>.tpbr
│   ├── <TaskRecordID_1>.tpbr
│   └── ...
└── ... 
```

---

**3) 记录链接拓扑**

![TPBench 记录链接拓扑](./arts/record_link_topology.svg)

`.tpbr` 文件使用对应条目 `.tpbe` 的 ID 命名，`corelib` 通过 ID 处理跨域和跨文件访问。

---

**4) 通用头部**

各种记录数据类型是 TPBench 记录的主要部分。每种类型由头部定义。`tpb_meta_header` 结构存储头部信息。每个 `.tpbr` 文件包含一个元数据块，其中包含描述记录数据的维度、数据类型和布局的头部 (`header[i]`)。

```c
typedef struct tpb_meta_header {
    uint32_t block_size;                          /**< 头部大小（字节） */
    uint32_t ndim;                                /**< 维度数，在 [0, TPBM_DATA_NDIM_MAX] 范围内 */
    uint64_t data_size;                           /**< 头部的记录数据大小（字节） */
    uint32_t type_bits;                           /**< 数据类型控制位：TPB_PARM_SOURCE_MASK | */
                                                  /**< TPB_PARM_CHECK_MASK | TPB_PARM_TYPE_MASK */
    uint32_t _reserve;                            /**< 保留填充以对齐 */
    uint64_t uattr_bits;                          /**< 指标单位编码，对齐到 TPB_UNIT_T */
    char name[TPBM_NAME_STR_MAX_LEN];            /**< 名称，长度在 [0, TPBM_NAME_STR_MAX_LEN] 范围内 */
    char note[TPBM_NOTE_STR_MAX_LEN];            /**< 注释和描述 */
    uint64_t dimsizes[TPBM_DATA_NDIM_MAX];       /**< 维度大小：dimsizes[0]=最内层 (d0) */
    char dimnames[TPBM_DATA_NDIM_MAX][64];       /**< 维度名称，与 dimsizes 平行 */
} tpb_meta_header_t;                              /**< 2840 字节（磁盘上固定大小） */
```

`dimsizes` 和 `dimnames` 数组是具有 `TPBM_DATA_NDIM_MAX`（7）个槽位的固定宽度内联数组。只有前 `ndim` 个槽位有意义；未使用的槽位填充为零。`dimnames` 中的每个槽位是 64 字节的固定宽度字符串。`dimsizes` 中的每个槽位存储该维度中的元素数量。

`type_bits` 字段使用 32 位掩码对数据类型进行编码：`TPB_PARM_SOURCE_MASK`（位 24-31）、`TPB_PARM_CHECK_MASK`（位 16-23）和 `TPB_PARM_TYPE_MASK`（位 0-15）。`uattr_bits` 字段使用 `tpb-unitdefs.h` 中的 `TPB_UNIT_T` 编码对指标单位进行编码。

对于多维记录，维度 0 是最内层维度。对于 dimsizes[0]=4 和 dimsizes[1]=2 的 2D 数组 X[2][4]，记录数据文件的布局为：
```
Address: 0x00    0x01    0x02    0x03    0x04    0x05    0x06    0x07
Data:    X[0][0] X[0][1] X[0][2] X[0][3] X[1][0] X[1][1] X[1][2] X[1][3]
```

---

**5) 时间戳**

`uint64_t utc_bits` 用于编码日期时间，并准备 ISO-8601 表示。`uint64_t btime` 用于存储高精度启动时间（自系统启动以来的纳秒），与日历日期时间无关。对于从启动时间秒到日历日期时间的粗略转换：
- 每天 24 小时
- 每月 730 小时（365/12 * 24）
- 每年 8760 小时（365 * 24）

`typedef uint64_t tpb_dtbits_t` 将日期时间和时区打包成紧凑的 64 位表示：

| 位范围 | 字段 | 大小 | 范围 |
|-----------|-------|------|-------|
| 0-5       | 秒 | 6 位 | 0-59 |
| 6-11      | 分钟 | 6 位 | 0-59 |
| 12-16     | 小时   | 5 位 | 0-23 |
| 17-21     | 天     | 5 位 | 1-31 |
| 22-25     | 月   | 4 位 | 1-12 |
| 26-33     | 年偏差 | 8 位 | 0-255 (1970-2225) |
| 34-41     | 时区 | 8 位 | 15 分钟增量 |
| 42-63     | 保留 | 22 位 | 未使用 |

编码方法：

- **年编码**：年存储为从 1970 开始的偏差（例如，2026 年存储为 56）。

- **时区编码**：时区偏差以 15 分钟增量存储。该值是有符号的（二进制补码），范围约为 -32 到 +31.75 小时。

- **字节序说明**：
位布局由位位置（0-63）定义，与主机字节序无关。字段访问通过位移和掩码操作执行，在大端和小端系统上产生相同的结果：

```c
/* 示例：提取秒（位 0-5） */
uint8_t sec = (uint8_t)((utc_bits >> 0) & 0x3F);  /* 在任何字节序上工作 */

/* 示例：提取年偏差（位 26-33） */
uint8_t year_bias = (uint8_t)((utc_bits >> 26) & 0xFF);
uint16_t year = 1970 + year_bias;
```

当 `utc_bits` 存储到磁盘或通过网络传输时，它存储为本机 `uint64_t` 值。读取器应将其视为不透明的 64 位整数，并使用上面显示的相同位提取操作。不要直接将内存转换为字节数组或假设特定的字节序。

---

**6) 记录类型和 ID**

每个 ID 是存储在 20 字节无符号 char 变量中的 20 字节 SHA1 哈希字符串。

| 记录类型 | ID | 描述 |
|-------------|-----------|-------------|
| **任务批次记录 (TBatch Record)** | `SHA1("tbatch" + <utc_bits> + <btime> + <hostname> + <username> + <front_end_pid>)` | 每次 tbatch 调用一条记录 |
| **Kernel 记录 (Kernel Record)** | `SHA1("kernel" + <kernel_name> + <so_sha1> + <bin_sha1>)` | 构建时检查或生成一次 |
| **任务记录 (Task Record)** | `SHA1("task" + <utc_bits> + <btime> + <hostname> + <username> + <tbatch_id> + <kernel_id> + <order_in_batch> + <pid> + <tid>)` | 每次 kernel 调用一条记录 |
| **评分记录 (Score Record)** | `SHA1("score" + <utc_bits> + <btime> + <hostname> + <username> + <score_name> + <calc_order>)` | 每个计算的评分或子评分一条记录 |

**Magic 签名**

Magic 签名是标记 `.tpbe` 和 `.tpbr` 边界的特殊 8 字节无符号 char 字符串，包括整个条目块的开始/结束、元块开始和记录数据块的开始/结束。Magic 签名的格式为：

| 字节位置 | 00 | 01 | 02 | 03 | 04 | 05 | 06 | 07 |
|---|---|---|---|---|---|---|---|---|
| 十六进制 | E1 | 54 | 50 | 42 | \<X\> | \<Y\> | 31 | E0 |
| ASCII 文本 | . | T | P | B | . | S/D/E | 1 | . |

**X**: 域标记，由两个 4 位标记构成：`0x<HI><LO>`。例如，tbatch `.tpbe` 文件的开始标记为 `0xE1 0x54 0x50 0x42 0xE0 0x53 0x31 0xE0`。
- HI：高 4 位。文件类型标记。`.tpbe: 0xE`；`.tpbr: 0xD`。
- LO：低 4 位。域类型标记。TBatch: `0`；kernel: `1`；task: `2`。

**Y**: 位置标记。文件开始：`0x53 ('S')`。文件结束：`0x45 ('E')`。块分隔符：`0x44 ('D')`。

### 2.2. 任务批次记录

任务批次 (tbatch) 是调用一个或多个 kernel 的执行上下文。每次 `tpbcli` 调用或专用 kernel 执行（例如直接调用 `tpbk_<stream>.x`）都会创建一个新的 tbatch。TBatchID 作为每个 tbatch 记录的链接 ID。

#### 2.2.1. 属性

```c
typedef struct tbatch_attr {
    unsigned char tbatch_id[20];        /**< TBatchID - 主链接 ID（20 字节 SHA-1） */
    unsigned char derive_to[20];           /**< 重复跟踪：0=无，否则指向其他 TBatchID */
    unsigned char inherit_from[20];         /**< 溯源：若由其他记录复制/派生则为源 TBatchID，否则为零 */
    tpb_dtbits_t utc_bits;              /**< 批次开始日期时间（64 位紧凑编码） */
    uint64_t btime;                     /**< 批次开始时的启动时间（自启动以来的纳秒） */
    uint64_t duration;                  /**< 批次总持续时间（纳秒） */
    char hostname[64];                  /**< 执行主机名，ASCII */
    char username[64];                  /**< 启动批次的用户名，ASCII */
    uint32_t front_pid;                 /**< 前端进程 ID */
    uint32_t nkernel;                   /**< 此批次中执行的非重复 kernel 数 */
    uint32_t ntask;                     /**< 此批次中的任务记录数 */
    uint32_t nscore;                    /**< 此批次中的评分记录数 */
    uint32_t batch_type;                /**< 0=run, 1=benchmark */
    uint32_t nheader;                   /**< 头部数量 */
    tpb_meta_header_t *headers;
} tbatch_attr_t;
```

实现程序负责在相应域文件中添加和管理固定头部。在任务批次记录中，有三个固定头部成员：
- header[0]: KernelRecordIDs
    - 20 字节无符号 char
    - tbatch 中执行的 kernel 的 KernelRecordIDs 的非重复列表，按启动时间从旧到新排序，从 0 到 nkernel-1。
    - ndim = 1, length = nkernel
- header[1]: TaskRecordIDs
    - 20 字节无符号 char
    - tbatch 中已完成任务的 TaskRecordIDs 的非重复列表。
    - ndim = 1, length = ntask
- header[2]: ScoreRecordIDs
    - 20 字节无符号 char
    - tbatch 中计算的基准评分的 ScoreRecordIDs 的非重复列表。
    - ndim = 1, length = nscore

**注意：**

**1) TBatchID 计算**

```
SHA1("tbatch" + <UTC 时间戳> + <机器启动纳秒数> + <主机名> + <用户名> + <前端进程 ID>)
```

示例：`SHA1("tbatch20250308T130801Z3600000000000node01testuser13249")`

#### 2.2.2. 条目结构 (.tpbe)

文件结构：
```
+----------------------0-+  
| meta_magic (8B)        |  <- 0xe1 'T' 'P' 'B' 0xe0 'S' 0x31 0xe0
+----------------------8-+ 
| entry[0:N] (264B)      |
+-----------------8+264N-+
| end_magic (8B)         |  <- 0xe1 'T' 'P' 'B' 0xe0 'E' 0x31 0xe0
+----------------16+264N-+
```


条目成员：
| 名称 | 大小 |
|---|---|
| tbatch_id | 20 |
| inherit_from | 20 |
| start_utc_bits | 8 |
| duration | 8 |
| hostname | 64 |
| nkernel | 4 |
| ntask | 4 |
| nscore | 4 |
| batch_type | 4 |
| reserve | 128（`TPB_RAF_RESERVE_SIZE`） |
| **总计** | **264** |

Magic 签名：

| Magic | 文本 | 十六进制值 | 用途 |
| ---------- | --- |------------------------------ | -------------------- |
| entry_begin_magic | . T P B . S 1 . | `E1 54 50 42 E0 53 31 E0` | 条目文件开始 |
| entry_end_magic | . T P B . E 1 . | `E1 54 50 42 E0 45 31 E0` | 条目文件结束 |

#### 2.2.3. 记录结构 (.tpbr)

`.tpbr` 记录文件由**元数据部分**和**记录数据部分**组成。元数据部分存储批次属性（来自 `tbatch_attr_t`，见 2.2.1）和描述记录数据布局的 3 个固定头部。记录数据部分根据固定头部定义的维度、大小和类型存储数据。

文件结构：
```
+--------------------0-+
| meta_magic           |  <- 8B
+--------------------8-+
| metasize             |  <- 8B, 元数据部分大小
+-------------------16-+
| datasize             |  <- 8B, 记录数据部分大小
+-------------------24-+
| tbatch_id            |  <- 20B
+-------------------44-+
| derive_to               |  <- 20B
+-------------------64-+
| inherit_from             |  <- 20B
+-------------------84-+
| utc_bits             |  <- 8B
+-------------------92-+
| btime                |  <- 8B
+------------------100-+
| duration             |  <- 8B
+------------------108-+
| hostname             |  <- 64B
+------------------172-+
| username             |  <- 64B
+------------------236-+
| front_pid            |  <- 4B
+------------------240-+
| nkernel              |  <- 4B
+------------------244-+
| ntask                |  <- 4B
+------------------248-+
| nscore               |  <- 4B
+------------------252-+
| batch_type           |  <- 4B
+------------------256-+
| nheader              |  <- 4B
+------------------260-+
| 128-Byte reserve     |  <- `TPB_RAF_RESERVE_SIZE`，不透明保留
+------------------388-+
| fixed_headers[i]     |  <- 3 x tpb_meta_header_t 和自定义头部
+-------------metasize-+
| record_magic         |  <- 8B
+-----------metasize+8-+
| record_data          |  <- 按头部中的维度、大小、类型存储
+--metasize+datasize-8-+
| end_magic            |  <- 8B
+----metasize+datasize-+
```

Magic 签名：

| Magic | 文本 | 十六进制值 | 用途 |
| ----------------- | --------------- | ------------------------- | -------------------- |
| meta_magic | . T P B . S 0 . | `E1 54 50 42 D0 53 31 E0` | 元数据部分开始 |
| record_magic | . T P B . D 0 . | `E1 54 50 42 D0 44 31 E0` | 记录数据部分 |
| end_magic | . T P B . E 0 . | `E1 54 50 42 D0 45 31 E0` | tpbr 结束 |

**注意**：ID 数组（TaskRecordIDs 和 ScoreRecordIDs）现在作为头部块存储在元数据部分内，而不是作为固定头部后的原始字节。

### 2.3. Kernel 记录

Kernel 域存储有关被评估 kernel 的元数据，包括描述、版本、参数定义和指标定义。每个唯一 kernel 一条记录（跨调用共享）。

#### 2.3.1. 属性

```c
typedef struct kernel_attr {
    unsigned char kernel_id[20];        /**< KernelID - 主链接 ID（20 字节 SHA-1），ASCII */
    unsigned char derive_to[20];           /**< 重复跟踪：0=无，否则指向其他 KernelID，ASCII */
    unsigned char inherit_from[20];         /**< 溯源：若由其他记录复制/派生则为源 KernelID，否则为零 */
    unsigned char src_sha1[20];         /**< 连接源文件的 SHA-1 哈希，ASCII */
    unsigned char so_sha1[20];          /**< 共享库文件的 SHA-1 哈希，ASCII */
    unsigned char bin_sha1[20];         /**< 可执行文件的 SHA-1 哈希，ASCII */

    char kernel_name[256];              /**< kernel 名称（不含 tpbk_前缀），utf-8 */
    char version[64];                   /**< 版本字符串，utf-8 */
    char description[2048];             /**< 人类可读描述，utf-8 */

    uint32_t nparm;                     /**< 注册的参数数量 */
    uint32_t nmetric;                   /**< 注册的输出指标数量 */
    uint32_t kctrl;                     /**< kernel 控制位（PLI=2） */
    uint32_t nheader;                   /**< 头部数量（= nparm + nmetric） */
    uint32_t reserve;                   /**< 填充至 8 字节对齐 */
} kernel_attr_t;
```

固定头部成员：
- header[0..nparm-1]: 参数定义
    - 每个 `tpb_meta_header_t` 描述一个参数：名称、dtype、ndim、dims。
    - 记录数据：每个参数的默认值。
    - ndim >= 1，长度由参数定义确定。
- header[nparm..nparm+nmetric-1]: 指标定义
    - 每个 `tpb_meta_header_t` 描述一个输出指标：名称、dtype、ndim、dims。
    - 记录数据：每个指标的单位编码。
    - ndim >= 1，长度由指标定义确定。

KernelID:
```
SHA1("kernel" + <kernel_name> + <so_sha1> + <bin_sha1>)
```

注意：
- `kctrl` 存放来自 `tpb-public.h` 的内核集成类型。仅支持 `TPB_KTYPE_PLI`。
- 对于规范记录，`derive_to` 全为零；否则它指向规范的 `kernel_id`。
- `inherit_from` 全为零，除非此记录由另一条 kernel 记录派生（溯源）。

#### 2.3.2. 条目结构 (.tpbe)

文件结构：
```
+----------------------0-+
| entry_begin_magic (8B) |  <- 0xe1 'T' 'P' 'B' 0xe1 'S' 0x31 0xe0
+----------------------8-+
| entry[0:N] (264B)      |
+-----------------8+264N-+
| entry_end_magic (8B)   |  <- 0xe1 'T' 'P' 'B' 0xe1 'E' 0x31 0xe0
+----------------16+264N-+
```

条目成员（`kernel_attr_t` 的简化子集）：

| 名称 | 大小 |
|---|---|
| kernel_id | 20 |
| inherit_from | 20 |
| kernel_name | 64 |
| so_sha1 | 20 |
| kctrl | 4 |
| nparm | 4 |
| nmetric | 4 |
| reserve | 128（`TPB_RAF_RESERVE_SIZE`） |
| **总计** | **264** |

Magic 签名：

| Magic | 文本 | 十六进制值 | 用途 |
| ----------------- | --------------- | ------------------------- | -------------------- |
| entry_begin_magic | . T P B . S 1 . | `E1 54 50 42 E1 53 31 E0` | 条目文件开始 |
| entry_end_magic   | . T P B . E 1 . | `E1 54 50 42 E1 45 31 E0` | 条目文件结束 |

#### 2.3.3. 记录结构 (.tpbr)

`.tpbr` 记录文件由**元数据部分**和**记录数据部分**组成。元数据部分存储 kernel 属性（来自 `kernel_attr_t`，见 2.3.1）和描述记录数据布局的固定头部。记录数据部分根据固定头部定义的维度、大小和类型存储数据。

文件结构：
```
+--------------------0-+
| meta_magic           |  <- 8B
+--------------------8-+
| metasize             |  <- 8B, 元数据部分大小
+-------------------16-+
| datasize             |  <- 8B, 记录数据部分大小
+-------------------24-+
| kernel_id            |  <- 20B
+-------------------44-+
| derive_to               |  <- 20B
+-------------------64-+
| inherit_from             |  <- 20B
+-------------------84-+
| src_sha1             |  <- 20B
+------------------104-+
| so_sha1              |  <- 20B
+------------------124-+
| bin_sha1             |  <- 20B
+------------------144-+
| kernel_name          |  <- 256B
+------------------400-+
| version              |  <- 64B
+------------------464-+
| description          |  <- 2048B
+-----------------2512-+
| nparm                |  <- 4B
+-----------------2516-+
| nmetric              |  <- 4B
+-----------------2520-+
| kctrl                |  <- 4B
+-----------------2524-+
| nheader              |  <- 4B
+-----------------2528-+
| 128-Byte reserve     |  <- `TPB_RAF_RESERVE_SIZE`，不透明保留
+-----------------2656-+
| fixed_headers[i]     |  <- (nparm + nmetric) x tpb_meta_header_t + 用户头部
+-------------metasize-+
| record_magic         |  <- 8B
+-----------metasize+8-+
| record_data          |  <- 按头部中的维度、大小、类型存储
+--metasize+datasize-8-+
| end_magic            |  <- 8B
+----metasize+datasize-+
```

头部成员：
- header[0..nparm-1]: 参数定义
- header[nparm..nparm+nmetric-1]: 指标定义

规则：
- 头部顺序是确定性的：所有参数在前，然后是所指标，然后是用户定义。

Magic 签名：

| Magic | 文本 | 十六进制值 | 用途 |
| ----------------- | --------------- | ------------------------- | -------------------- |
| meta_magic | . T P B . S 1 . | `E1 54 50 42 D1 53 31 E0` | 元数据部分开始 |
| record_magic | . T P B . D 1 . | `E1 54 50 42 D1 44 31 E0` | 记录数据部分 |
| end_magic | . T P B . E 1 . | `E1 54 50 42 D1 45 31 E0` | tpbr 结束 |

### 2.4. 任务记录

任务记录域存储单次 kernel 调用的输入参数和输出指标。每次 handle 执行一条记录。

#### 2.4.1. 属性

```c
typedef struct task_attr {
    unsigned char task_record_id[20];   /**< TaskRecordID - 主链接 ID（20 字节 SHA-1） */
    unsigned char derive_to[20];           /**< 重复跟踪：0=无，否则指向其他 TaskRecordID */
    unsigned char inherit_from[20];         /**< 溯源：若由其他记录复制/派生则为源 TaskRecordID，否则为零 */
    unsigned char tbatch_id[20];        /**< 外键：链接到产生此记录的任务批次 */
    unsigned char kernel_id[20];        /**< 外键：链接到 kernel 定义记录 */

    tpb_dtbits_t utc_bits;              /**< 调用日期时间（64 位紧凑编码） */
    uint64_t btime;                     /**< 调用时的启动时间（自启动以来的纳秒） */
    uint64_t duration;                  /**< kernel 执行持续时间（纳秒） */

    uint32_t exit_code;                 /**< kernel 退出代码（0=成功） */
    uint32_t handle_index;              /**< 批次内的 handle 索引（从 0 开始） */
    uint32_t pid;                       /**< 写入进程 ID */
    uint32_t tid;                       /**< 写入线程 ID */
    uint32_t ninput;                    /**< 输入参数头部数量 */
    uint32_t noutput;                   /**< 输出指标头部数量 */
    uint32_t nheader;                   /**< 头部数量（= ninput + noutput） */
    uint32_t reserve;                   /**< 填充至 8 字节对齐 */
} task_attr_t;
```

固定头部成员：
- header[0..ninput-1]: 输入参数
    - 每个 `tpb_meta_header_t` 描述一个输入参数：名称、dtype、ndim、dims。
    - 记录数据：此调用中使用的实际参数值。
    - ndim >= 1，长度由参数定义确定。
- header[ninput..ninput+noutput-1]: 输出指标
    - 每个 `tpb_meta_header_t` 描述一个输出指标：名称、dtype、ndim、dims。
    - 记录数据：此调用中测量的实际结果。
    - ndim >= 1，长度由指标定义确定。

TaskRecordID:
```
SHA1("task" + <utc_bits> + <btime> + <hostname> + <username> + <tbatch_id> + <kernel_id> + <order_in_batch> + <pid> + <tid>)
```

注意：
- `task_record_id` 的唯一性由完整哈希成分限定，在工作空间中应全局唯一。
- 启用去重时，`derive_to` 指向规范的任务记录。
- `inherit_from` 全为零，除非此记录由另一条任务记录派生（溯源）。

#### 2.4.2. 条目结构 (.tpbe)

文件结构：
```
+----------------------0-+
| entry_begin_magic (8B) |  <- 0xe1 'T' 'P' 'B' 0xe2 'S' 0x31 0xe0
+----------------------8-+
| entry[0:N] (232B)      |
+-----------------8+232N-+
| entry_end_magic (8B)   |  <- 0xe1 'T' 'P' 'B' 0xe2 'E' 0x31 0xe0
+----------------16+232N-+
```

条目成员（`task_attr_t` 的简化子集）：

| 名称 | 大小 |
|---|---|
| task_record_id | 20 |
| inherit_from | 20 |
| tbatch_id | 20 |
| kernel_id | 20 |
| utc_bits | 8 |
| duration | 8 |
| exit_code | 4 |
| handle_index | 4 |
| reserve | 128（`TPB_RAF_RESERVE_SIZE`） |
| **总计** | **232** |

Magic 签名：

| Magic | 文本 | 十六进制值 | 用途 |
| ----------------- | --------------- | ------------------------- | -------------------- |
| entry_begin_magic | . T P B . S 1 . | `E1 54 50 42 E2 53 31 E0` | 条目文件开始 |
| entry_end_magic   | . T P B . E 1 . | `E1 54 50 42 E2 45 31 E0` | 条目文件结束 |

#### 2.4.3. 记录结构 (.tpbr)

`.tpbr` 记录文件由**元数据部分**和**记录数据部分**组成。元数据部分存储任务属性（来自 `task_attr_t`，见 2.4.1）和描述记录数据布局的固定头部。记录数据部分根据固定头部定义的维度、大小和类型存储数据。

文件结构：
```
+--------------------0-+
| meta_magic           |  <- 8B
+--------------------8-+
| metasize             |  <- 8B, 元数据部分大小
+-------------------16-+
| datasize             |  <- 8B, 记录数据部分大小
+-------------------24-+
| task_record_id       |  <- 20B
+-------------------44-+
| derive_to               |  <- 20B
+-------------------64-+
| inherit_from             |  <- 20B
+-------------------84-+
| tbatch_id            |  <- 20B
+------------------104-+
| kernel_id            |  <- 20B
+------------------124-+
| utc_bits             |  <- 8B
+------------------132-+
| btime                |  <- 8B
+------------------140-+
| duration             |  <- 8B
+------------------148-+
| exit_code            |  <- 4B
+------------------152-+
| handle_index         |  <- 4B
+------------------156-+
| pid                  |  <- 4B
+------------------160-+
| tid                  |  <- 4B
+------------------164-+
| ninput               |  <- 4B
+------------------168-+
| noutput              |  <- 4B
+------------------172-+
| nheader              |  <- 4B
+------------------176-+
| reserve              |  <- 4B
+------------------180-+
| 128-Byte reserve     |  <- `TPB_RAF_RESERVE_SIZE`，不透明保留
+------------------308-+
| headers[i]           |  <- (ninput + noutput) x tpb_meta_header_t + 用户头部
+-------------metasize-+
| record_magic         |  <- 8B
+-----------metasize+8-+
| record_data          |  <- 按头部中的维度、大小、类型存储
+--metasize+datasize-8-+
| end_magic            |  <- 8B
+----metasize+datasize-+
```

头部成员：
- header[0..ninput-1]: 输入参数数据头部
- header[ninput..ninput+noutput-1]: 输出指标数据头部

规则：
- 头部顺序是确定性的：所有输入头部在前，然后是输出头部，然后是用户定义。
- `record_data` 按与其头部相同的顺序存储头部载荷。

Magic 签名：

| Magic | 文本 | 十六进制值 | 用途 |
| ----------------- | --------------- | ------------------------- | -------------------- |
| meta_magic | . T P B . S 1 . | `E1 54 50 42 D2 53 31 E0` | 元数据部分开始 |
| record_magic | . T P B . D 1 . | `E1 54 50 42 D2 44 31 E0` | 记录数据部分 |
| end_magic | . T P B . E 1 . | `E1 54 50 42 D2 45 31 E0` | tpbr 结束 |

### 2.5. 评分记录

**TODO: 当前版本未实现此部分，需要进一步设计。**

评分记录域存储通过预定义公式（例如来自 `stream.yaml`）计算的基准评分。每个评分或子评分计算一条记录。

| 键 | 定义 | 数据类型 |
|-----|------------|----------|
| ScoreRecordID | 评分记录的唯一 SHA-1 ID | string |
| TBatchID | 父任务批次 ID（外键） | string |
| ScoreName | 计算评分的名称 | string |
| ScoreValue | 评分的数值 | double |
| BenchFile | 基准定义 YAML 文件的路径 | string |
| ScoreTimeUTC | 计算评分的时间戳 | string |

**链接 ID 公式**：`SHA1("score" + <UTC 时间戳> + <机器启动纳秒数> + <主机名> + <用户名> + <评分名称> + <计算顺序>)`

---

### 2.6. OS 记录

**TODO: 当前版本未实现此部分，需要进一步设计。**

操作系统域存储操作系统的静态信息和运行时配置/状态。每个 tbatch 一条记录（如果 `auto_collect=false` 则频率更低）。

| 键 | 定义 | 数据类型 |
|-----|------------|----------|
| OSID | OS 记录的唯一 SHA-1 ID | string |
| OSType | 操作系统家族 (Linux/Windows/macOS/BSD) | string |
| Distribution | OS 发行版名称（例如 Ubuntu、CentOS） | string |
| Version | OS 版本字符串 | string |
| KernelVersion | kernel 版本（对于类 Unix 系统） | string |

**链接 ID 公式**：`SHA1("os" + <UTC 时间戳> + <主机名> + <kernel 版本>)`

---

### 2.7. CPU 记录

**TODO: 当前版本未实现此部分，需要进一步设计。**

CPU 静态域存储有关 CPU 的静态信息，包括型号名称、SKU ID、供应商和运行时配置。这些值在 tbatch 执行期间不会更改。每个系统一条记录（在 tbatches 之间重用）。

| 键 | 定义 | 数据类型 |
|-----|------------|----------|
| CPUStaticID | CPU 静态记录的唯一 SHA-1 ID | string |
| VendorID | CPU 供应商标识符（例如 GenuineIntel、AuthenticAMD） | string |
| ModelName | 人类可读的型号名称 | string |
| Family | CPU 家族编号 | int32_t |
| Model | CPU 型号编号 | int32_t |
| Stepping | CPU 步进修订 | int32_t |
| NumPhysicalCores | 物理核心数 | int32_t |
| NumLogicalCores | 逻辑处理器数（线程） | int32_t |
| BaseFrequency | 基础时钟频率（MHz） | uint32_t |
| MaxTurboFrequency | 最大睿频（MHz） | uint32_t |
| L1CacheSize | 每核心 L1 缓存大小（KB） | uint32_t |
| L2CacheSize | 每核心 L2 缓存大小（KB） | uint32_t |
| L3CacheSize | L3（共享）缓存大小（MB） | uint32_t |
| HBMSize | 片上 HBM 大小（MB） | uint32_t |
| ISAFlags | 支持的指令集标志（例如 AVX、AVX2、SSE4.2） | string |

**链接 ID 公式**：`SHA1("cpu_static" + <UTC 时间戳> + <主机名> + <供应商 ID>)`

---

### 2.8. 内存记录

**TODO: 当前版本未实现此部分，需要进一步设计。**

内存静态域存储有关系统内存配置的静态信息。这些值在 tbatch 执行期间不会更改。每个系统一条记录（在 tbatches 之间重用）。

| 键 | 定义 | 数据类型 |
|-----|------------|----------|
| MemoryStaticID | 内存静态记录的唯一 SHA-1 ID | string |
| TotalSize | 总安装内存（MB） | uint64_t |
| NChannel | 内存通道数（例如双通道为 2） | int32_t |
| ChannelType | 内存类型（DDR3/DDR4/DDR5/LRDIMM） | string |
| MainFrequency | 工作频率（MHz） | uint32_t |
| NModule | 安装的 DIMM 模块数 | int32_t |
| ModuleSize | 各个模块大小的 JSON 数组 | string |
| ECCFlags | ECC 技术标志，非 ECC 内存为 "None" 或 "N/A"。 | string |

**链接 ID 公式**：`SHA1("memory_static" + <UTC 时间戳> + <主机名> + <总容量 MB>)`

---

### 2.9. 加速器记录

**TODO: 当前版本未实现此部分，需要进一步设计。**

加速器静态域存储有关加速器（GPU、FPGA 等）的静态信息。每个加速器设备一条记录。

| 键 | 定义 | 数据类型 |
|-----|------------|----------|
| AcceleratorStaticID | 加速器静态记录的唯一 SHA-1 ID | string |
| DeviceType | 加速器类型（GPU/FPGA/ASIC） | string |
| Vendor | 制造商名称（例如 NVIDIA、AMD、Intel） | string |
| DeviceName | 型号/产品名称 | string |
| DriverVersion | 驱动版本字符串 | string |
| NDevice | 系统中此类设备数 | int32_t |
| MemorySize | 设备内存大小（MB） | uint64_t |
| NCore | 计算核心/单元数 | int32_t |
| BaseFrequency | 基础时钟频率（MHz） | uint32_t |

**链接 ID 公式**：`SHA1("accelerator_static" + <UTC 时间戳> + <主机名> + <供应商> + <设备名称>)`

---

### 2.10. 网络记录

**TODO: 当前版本未实现此部分，需要进一步设计。**

网络静态域存储有关网络适配器和接口的聚合信息。这些值在 tbatch 执行期间不会更改。每个系统一条记录（在 tbatches 之间重用）。

| 键 | 定义 | 数据类型 |
|-----|------------|----------|
| NetworkStaticID | 网络静态记录的唯一 SHA-1 ID | string |
| NInterface | 网络接口总数 | int32_t |
| IFTrios | 接口详细信息的 JSON 数组（名称、MAC、速度） | string |

**链接 ID 公式**：`SHA1("network_static" + <UTC 时间戳> + <主机名> + <主 MAC 地址>)`

---

### 2.11. 主板记录

**TODO: 当前版本未实现此部分，需要进一步设计。**

主板静态域存储有关主板的静态信息。这些值在 tbatch 执行期间不会更改。每个系统一条记录（在 tbatches 之间重用）。

| 键 | 定义 | 数据类型 |
|-----|------------|----------|
| MotherBoardStaticID | 主板静态记录的唯一 SHA-1 ID | string |
| Manufacturer | 主板制造商名称 | string |
| Model | 主板型号 | string |
| BIOSVersion | BIOS/UEFI 固件版本 | string |
| BIOSDateUTC | ISO 8601 格式的 BIOS 发布日期 | string |

**链接 ID 公式**：`SHA1("motherboard_static" + <UTC 时间戳> + <主机名> + <制造商> + <型号>)`

---

### 2.12. 机箱记录

**TODO: 当前版本未实现此部分，需要进一步设计。**

机箱静态域存储有关系统机箱/外壳的静态信息。这些值在 tbatch 执行期间不会更改。每个系统一条记录（在 tbatches 之间重用）。

| 键 | 定义 | 数据类型 |
|-----|------------|----------|
| ChassisStaticID | 机箱静态记录的唯一 SHA-1 ID | string |
| Type | 机箱类型（Tower/Rack/Blade/Laptop/Desktop/Cluster） | string |
| Manufacturer | 机箱制造商名称 | string |
| Model | 机箱型号描述 | string |
| CoolType | 冷却系统类型 | string |

**链接 ID 公式**：`SHA1("chassis_static" + <UTC 时间戳> + <主机名> + <制造商> + <型号>)`

## 3. 记录后端：`rafdb`

### 3.1. 模块结构

`rafdb` 后端实现为 `src/corelib/rafdb/` 中的一组 C 源文件：

```
src/corelib/rafdb/
├── tpb-raf-types.h      # 内部常量、magic 定义、文件路径定义
├── tpb-raf-workspace.c  # 工作空间解析、初始化、config.json 读写
├── tpb-raf-magic.c      # Magic 签名构建、验证、扫描
├── tpb-raf-entry.c      # 所有 3 个域的 .tpbe 追加/读取/列表
├── tpb-raf-record.c     # 所有 3 个域的 .tpbr 写入/读取
├── tpb-raf-id.c         # 基于 SHA1 的 ID 生成
├── tpb-sha1.c             # 纯 C SHA1 实现（RFC 3174）
└── tpb-sha1.h             # SHA1 内部头文件
```

所有公共类型和函数声明都在 `src/include/tpb-public.h` 中，通过 `src/include/tpbench.h` 公开。

### 3.2. 工作空间解析

`tpb_raf_resolve_workspace()` 在每个 `tpbcli` 子命令开始时调用。解析顺序：

1. 如果 `$TPB_WORKSPACE` 已设置且非空，使用该目录。
2. 如果 `$HOME/.tpbench/etc/config.json` 存在且包含 `"name"` 字段，使用 `$HOME/.tpbench/`。
3. 否则，在 `$HOME/.tpbench/` 创建默认工作空间，包含 `etc/config.json`（`{"name": "default"}`）和 `rafdb/{task_batch,kernel,task}/` 目录。

### 3.3. API 摘要

工作空间：
- `tpb_raf_resolve_workspace(out_path, pathlen)` -- 解析工作空间路径
- `tpb_raf_init_workspace(workspace_path)` -- 创建目录结构和配置

Magic:
- `tpb_raf_build_magic(ftype, domain, pos, out)` -- 构建 8 字节 magic
- `tpb_raf_validate_magic(magic, ftype, domain, pos)` -- 验证 magic 字节
- `tpb_raf_magic_scan(buf, len, offsets, nfound, max)` -- 扫描缓冲区中的 magic 签名

条目 (.tpbe):
- `tpb_raf_entry_append_{tbatch,kernel,task}(workspace, entry)` -- 追加条目
- `tpb_raf_entry_list_{tbatch,kernel,task}(workspace, entries, count)` -- 列出所有条目

记录 (.tpbr):
- `tpb_raf_record_write_{tbatch,kernel,task}(workspace, attr, data, datasize)` -- 写入记录
- `tpb_raf_record_read_{tbatch,kernel,task}(workspace, id, attr, data, datasize)` -- 读取记录
- `tpb_raf_free_headers(headers, nheader)` -- 释放分配的头部数组

ID 生成：
- `tpb_raf_gen_tbatch_id(utc_bits, btime, hostname, username, pid, id_out)` -- TBatchID
- `tpb_raf_gen_kernel_id(name, so_sha1, bin_sha1, id_out)` -- KernelID
- `tpb_raf_gen_task_id(utc_bits, btime, hostname, username, tbatch_id, kernel_id, order, pid, tid, id_out)` -- TaskRecordID
- `tpb_raf_id_to_hex(id, hex)` -- 将 20 字节 ID 转换为 40 字符十六进制字符串

### 3.4. 头部序列化

每个 `tpb_meta_header_t` 的磁盘格式（固定 2840 字节）：

```
block_size       (4B)
ndim             (4B)
data_size        (8B)
type_bits        (4B)   -- TPB_PARM_SOURCE_MASK | TPB_PARM_CHECK_MASK | TPB_PARM_TYPE_MASK
_reserve         (4B)   -- 保留填充
uattr_bits       (8B)   -- 指标单位编码 (TPB_UNIT_T)
name             (256B)
note             (2048B)
dimsizes[0..6]   (56B)  -- 7 x uint64_t，只有前 ndim 个槽位有意义
dimnames[0..6]   (448B) -- 7 x 64 字节字符串，与 dimsizes 平行
```

### 3.5. 内存安全

- 条目结构（`tbatch_entry_t` / `kernel_entry_t` 264 字节，`task_entry_t` 240 字节含尾部填充）为栈分配或调用者提供。
- `tpb_meta_header_t` 数组在记录读取期间堆分配；通过 `tpb_raf_free_headers()` 释放。
- 所有指针参数均进行 NULL 检查；失败时返回 `TPBE_NULLPTR_ARG`。
- 文件 I/O 使用显式大小检查；无缓冲区溢出。

## 4. 记录前端

记录前端实现在 `src/tpbcli_record.c` 中，通过 `src/tpbcli.c` 中的 `record`（或 `r`）子命令分发。

### 4.1. 列出记录

`tpbcli record list`（别名：`tpbcli record ls`、`tpbcli r list`、`tpbcli r ls`）列出工作空间中最新的 20 条 tbatch 条目，按开始时间排序（最新的在前）。

```bash
$ tpbcli record list
### --- 输出 ---
TPBench v1.0
YYYY-MM-DDThh:mm:ss [I] TPB_WORKSPACE: /home/user/.tpbench
YYYY-MM-DDThh:mm:ss [I] Reading records ... Done
YYYY-MM-DDThh:mm:ss [I] List of latest 20 tbatch records
===
Start Time (UTC)     TBatch ID                                 Type        NTask  NKernel  NScore  Duration (s)
2026-03-15T08:11:30Z a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2 run             3        2       0         1.234
2026-03-14T10:05:00Z f1e2d3c4b5a6f1e2d3c4b5a6f1e2d3c4b5a6f1e2 benchmark       6        3       0        12.567
===
```

列详情：

| 列 | 来源 | 格式 |
|--------|--------|--------|
| Start Time (UTC) | `start_utc_bits` 解码 | `YYYY-MM-DDThh:mm:ssZ` (ISO 8601) |
| TBatch ID | `tbatch_id` | 40 字符十六进制字符串 |
| Type | `batch_type` | `run` (0) 或 `benchmark` (1) |
| NTask | `ntask` | 整数 |
| NKernel | `nkernel` | 整数 |
| NScore | `nscore` | 整数（目前始终为 0） |
| Duration (s) | `duration` 纳秒 | 秒，3 位小数 |
