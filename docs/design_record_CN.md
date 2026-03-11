# 数据记录组件设计

TPBench记录影响kernel测试输出的数据，并捕获kernel调用产生的结果。由于实际性能对程序上下文和底层环境高度敏感，TPBench为每条记录标记专用ID，以便与其他kernel和环境因素结合进行进一步分析。

## 1. 核心概念

**任务批次 (Task Batch, TBatch)**: 任务批次(tbatch)是由单次TPBench前端调用发起的执行上下文（例如`tpbcli run`）。一个tbatch可以执行一个或多个kernel调用，每个调用都有自己的输入参数。tbatch内的所有kernel调用共享同一个TBatchID，该ID标记执行期间产生的所有记录。为了获得有效的测试结果，tbatch需要与实际场景相匹配的稳定运行时环境。具体来说：
- 如果用户使用脚本顺序调用多个`tpbcli`命令，每次调用应被视为单独的tbatch。
- 如果底层硬件或软件配置在调用之间发生变化，每种配置状态构成一个单独的tbatch。（请不要自找麻烦。）

**TBatchID**: 任务批次的唯一标识符，生成方式如下：
```
SHA1("tbatch" + <UTC时间戳> + <机器启动纳秒数> + <主机名> + <用户名> + <前端进程ID>)
```
其中`<machine_start_nanoseconds>`是自系统启动以来的纳秒数(uint64_t)。
示例：`SHA1("tbatch20250308T130801Z3600000000000node01testuser13249")`

---

**Kernel**: kernel是用户希望评估的程序或代码模块。kernel位于`${TPB_HOME}/lib`或`${TPB_WORKSPACE}/lib/`目录下，命名为`tpbk_<kernel_name>.<so|x>`。

---

**工作负载 (Workload)**: 工作负载是用于基准测试的kernel的具体实例化，由以下因素定义：
- kernel名称（标识要执行的代码）
- 输入参数（分配给kernel参数定义的值）
- 数据大小（问题规模，例如数组长度、内存大小）

工作负载代表基准测试执行的*可配置*方面。硬件状态、操作系统配置和其他环境因素*不属于*工作负载定义的一部分，而是作为执行上下文单独记录。

---

**参数 (Parameter)**: 参数是由kernel定义的命名、类型化的占位符，代表可能影响测量结果的可配置因素。参数具有：
- 名称（例如`ntest`、`total_memsize`）
- 数据类型（int、double、string等）
- 默认值
- 可选的验证约束（范围、列表、自定义）

---

**参数值 (Argument)**: 参数值是在工作负载执行期间提供给参数的具体值（数字、字符串或数据引用）。参数值通过CLI选项（`--kargs`、`--kenvs`、`--kmpiargs`）或配置文件提供。

---

**目标指标 (Target Metric)**: 目标指标是用户希望从工作负载执行中测量、观察或导出的量。最常见的指标是运行时间。在TPBench中，指标还可能包括：
- 性能指标（吞吐量、延迟、FLOPS）
- 正确性、准确性或其他算术精度指标
- 资源使用（功耗、内存带宽）
- 来自工作负载的任何用户定义的观测值

---

**域/记录 (Domain / Record)**: 记录存储影响kernel测试输出的数据和kernel调用产生的结果。每条记录包括：
- **链接ID (Link ID)**: 该记录的唯一标识符；其他记录引用它来建立关系。
- **固定键和值**: TPBench为每个域定义了几个固定名称的键，以支持跨平台的类似参数追踪。
- **动态键和值**: 许多硬件和软件设计都有特定参数；TPBench允许向记录添加自定义键。

每条记录属于一个域，该域将不同类型的记录分类到不同的方面，并允许在不同场景中获取和记录不同的数据。

---

**记录类型和ID**

| 记录类型 | ID格式 | 描述 |
|----------|--------|------|
| **任务记录 (Task Record)** | `SHA1("task" + <UTC时间戳> + <机器启动纳秒数> + <主机名> + <用户名> + <kernel名称> + <进程ID> + <批次中的顺序>)` | 每次kernel调用一条记录 |
| **评分记录 (Score Record)** | `SHA1("score" + <UTC时间戳> + <机器启动纳秒数> + <主机名> + <用户名> + <评分名称> + <计算顺序>)` | 每个计算的评分或子评分一条记录 |

## 2. TPBench Corelib中的数据记录逻辑设计

### 2.1. 数据库的逻辑组织

TPBench的记录存储和管理在工作空间下，除了特殊的跨工作空间工具外。TPBench使用两个环境变量来导航文件系统：`TPB_HOME`是TPBench安装路径，`TPB_WORKSPACE`是TPBench项目的根文件夹。

**工作空间解析：**
1. 如果明确设置了`${TPB_WORKSPACE}`环境变量，则使用该目录作为工作空间。
2. 否则，在`$TPB_HOME/.tpb_workspace/`中搜索工作空间配置文件。
3. 如果未找到，则在`$HOME/.tpb_workspace/`中搜索。

有关构建系统和文件系统结构的更多信息，请参阅[design_build.md](./design_build.md)。

---

**设计原理：**

目标指标是计算硬件状态、软件栈配置和工作负载输入参数/数据复杂组合的结果测量。理论上，可以对整个系统进行快照以及目标指标，构建一个涵盖性能、功耗、可移植性、准确性和弹性特性的综合数据库。然而，计算系统中开销和观察粒度之间的权衡是不可避免的。

因此，TPBench使用多个**数据域**来记录系统状态、kernel定义、输入参数、目标指标和相关因素。不同的域通过称为**链接ID (Link IDs)**的SHA-1 ID链接（类似于关系数据库中的主键）。这种方法避免了为每次测试记录完整的快照，同时保持了可追溯性。

每个域具有：
- **静态键**: TPBench建立的固定名称和定义
- **动态键**: 可定制的字段以适应不同的硬件和软件设计

每个域记录系统状态的特定方面，表征任务批次和kernel调用的状态、输入和输出。然而，在每次kernel调用时记录所有域会导致显著的开销。因此，`${TPB_WORKSPACE}/etc/config.json`中的设置可以控制记录行为：
- `auto_collect`: 布尔标志，用于启用/禁用域的自动收集
- `action`: 记录触发条件（例如`"kernel_invoke"`、`"user_invoke"`）

每个记录域有两种文件类型：
- 域头部文件 (`<DomainName>.binh`): 记录工作空间中每个单条记录的头部，以8字节TPBench域magic签名开头。
- 域记录文件 (`<RecordID>.bin`): 动态记录数据，由前端注释、元数据和数据构成。

---



**域的静态键：**

**1) 域：任务批次 (TaskBatch)**

任务批次(tbatch)是调用一个或多个kernel的执行上下文。每次`tpbcli`调用或专用kernel执行（例如直接调用`tpbk_<stream>.x`）都会创建一个新的tbatch。TBatchID作为每个tbatch记录的链接ID。

| 键 | 定义 | 数据类型 |
|-----|------------|----------|
| TBatchID | 任务批次SHA-1链接ID | string |
| TaskRecordIDn | 此批次内的任务记录ID列表 | list<string> |
| ScoreRecordIDs | 此批次内的评分记录ID列表 | list<string> |
| StartTimeUTC | ISO 8601格式的开始时间 (YYYY-MM-DDThh:mm:ssZ) | string |
| StartMachineTime | 自启动以来的纳秒数开始时间 | uint64_t |
| Duration | tbatch总持续时间（纳秒） | uint64_t |
| Hostname | tbatch执行的主机名 | string |
| User | 启动tbatch的用户名 | string |

**链接ID公式：** `SHA1("tbatch" + <UTC时间戳> + <机器启动纳秒数> + <主机名> + <用户名> + <前端进程ID>)`

---

**2) 域：Kernel**

Kernel域存储有关被评估kernel的元数据，包括描述、版本、参数定义和度量定义。每个唯一的kernel一条记录（跨调用共享）。

| 键 | 定义 | 数据类型 |
|-----|------------|----------|
| KernelID | Kernel域的唯一标识符 | string |
| KernelName | kernel名称（不含`tpbk_`前缀） | string |
| Version | kernel版本字符串 | string |
| Description | kernel用途的人类可读描述 | string |
| SourceFile | 编译所用源文件的路径 | string |
| CompileDateUTC | ISO 8601格式的编译时间戳 | string |
| ParameterDefinitions | 描述输入参数的JSON数组 | string |
| MetricDefinitions | 定义可测量输出的JSON数组 | string |

**链接ID公式：** `SHA1("kernel_def" + <kernel名称> + <版本> + <源文件哈希>)`

---

**3) 域：任务记录 (TaskRecord)**

任务记录域存储单次kernel调用的输入参数和输出指标。每次handle执行一条记录。

| 键 | 定义 | 数据类型 |
|-----|------------|----------|
| TaskRecordID | 任务记录的唯一SHA-1 ID | string |
| TBatchID | 链接到tbatch记录 | string |
| KernelID | 链接到kernel记录 | string |
| StartTimeUTC | ISO 8601格式的调用开始时间 | string |
| StartMachineTime | 自启动以来的纳秒数开始时间 | uint64_t |
| Duration | 执行持续时间（纳秒） | uint64_t |
| ExitCode | kernel退出代码（0 = 成功） | int32_t |

**任务记录ID公式：** `SHA1("task" + <UTC时间戳> + <机器启动纳秒数> + <持续时间> + <主机名> + <用户名> + <kernel名称> + <进程ID> + <批次中的顺序>)`

---

**4) 域：评分记录 (ScoreRecord)**

评分记录域存储通过预定义公式计算的基准评分（例如来自`stream.yaml`）。每个评分或子评分计算一条记录。

| 键 | 定义 | 数据类型 |
|-----|------------|----------|
| ScoreRecordID | 评分记录的唯一SHA-1 ID | string |
| TBatchID | 父任务批次ID（外键） | string |
| ScoreName | 计算评分的名称 | string |
| ScoreValue | 评分的数值 | double |
| BenchFile | 基准定义YAML文件的路径 | string |
| ScoreTimeUTC | 计算评分的时间戳 | string |

**链接ID公式：** `SHA1("score" + <UTC时间戳> + <机器启动纳秒数> + <主机名> + <用户名> + <评分名称> + <计算顺序>)`

---

**5) 域：操作系统 (OS)**

操作系统域存储操作系统的静态信息和运行时配置/状态。每个tbatch一条记录（如果`auto_collect=false`则频率更低）。

| 键 | 定义 | 数据类型 |
|-----|------------|----------|
| OSID | OS记录的唯一SHA-1 ID | string |
| OSType | 操作系统家族 (Linux/Windows/macOS/BSD) | string |
| Distribution | OS发行版名称（例如Ubuntu、CentOS） | string |
| Version | OS版本字符串 | string |
| KernelVersion | kernel版本（对于类Unix系统） | string |

**链接ID公式：** `SHA1("os" + <UTC时间戳> + <主机名> + <kernel版本>)`

---

**6) 域：CPU静态信息 (CPUStatic)**

CPU静态信息域存储有关CPU的静态信息，包括型号名称、SKU ID、供应商和运行时配置。这些值在tbatch执行期间不会更改。每个系统一条记录（在tbatches之间重用）。

| 键 | 定义 | 数据类型 |
|-----|------------|----------|
| CPUStaticID | CPU静态记录的唯一SHA-1 ID | string |
| VendorID | CPU供应商标识符（例如GenuineIntel、AuthenticAMD） | string |
| ModelName | 人类可读的型号名称 | string |
| Family | CPU家族编号 | int32_t |
| Model | CPU型号编号 | int32_t |
| Stepping | CPU步进修订 | int32_t |
| NumPhysicalCores | 物理核心数 | int32_t |
| NumLogicalCores | 逻辑处理器数（线程） | int32_t |
| BaseFrequency | 基础时钟频率（MHz） | uint32_t |
| MaxTurboFrequency | 最大睿频（MHz） | uint32_t |
| L1CacheSize | 每核心L1缓存大小（KB） | uint32_t |
| L2CacheSize | 每核心L2缓存大小（KB） | uint32_t |
| L3CacheSize | L3（共享）缓存大小（MB） | uint32_t |
| HBMSize | 片上HBM大小（MB） | uint32_t |
| ISAFlags | 支持的指令集标志（例如AVX、AVX2、SSE4.2） | string |

**链接ID公式：** `SHA1("cpu_static" + <UTC时间戳> + <主机名> + <供应商ID>)`

---

**7) 域：内存静态信息 (MemoryStatic)**

内存静态信息域存储有关系统内存配置的静态信息。这些值在tbatch执行期间不会更改。每个系统一条记录（在tbatches之间重用）。

| 键 | 定义 | 数据类型 |
|-----|------------|----------|
| MemoryStaticID | 内存静态记录的唯一SHA-1 ID | string |
| TotalSize | 总安装内存（MB） | uint64_t |
| NChannel | 内存通道数（例如双通道为2） | int32_t |
| ChannelType | 内存类型（DDR3/DDR4/DDR5/LRDIMM） | string |
| MainFrequency | 工作频率（MHz） | uint32_t |
| NModule | 安装的DIMM模块数 | int32_t |
| ModuleSize | 各个模块大小的JSON数组 | string |
| ECCFlags | ECC技术标志，非ECC内存为"None"或"N/A"。 | string |

**链接ID公式：** `SHA1("memory_static" + <UTC时间戳> + <主机名> + <总容量MB>)`

---

**8) 域：加速器静态信息 (AcceleratorStatic)**

加速器静态信息域存储有关加速器（GPU、FPGA等）的静态信息。每个加速器设备一条记录。

| 键 | 定义 | 数据类型 |
|-----|------------|----------|
| AcceleratorStaticID | 加速器静态记录的唯一SHA-1 ID | string |
| DeviceType | 加速器类型（GPU/FPGA/ASIC） | string |
| Vendor | 制造商名称（例如NVIDIA、AMD、Intel） | string |
| DeviceName | 型号/产品名称 | string |
| DriverVersion | 驱动版本字符串 | string |
| NDevice | 系统中此类设备数 | int32_t |
| MemorySize | 设备内存大小（MB） | uint64_t |
| NCore | 计算核心/单元数 | int32_t |
| BaseFrequency | 基础时钟频率（MHz） | uint32_t |

**链接ID公式：** `SHA1("accelerator_static" + <UTC时间戳> + <主机名> + <供应商> + <设备名称>)`

---

**9) 域：网络静态信息 (NetworkStatic)**

网络静态信息域存储有关网络适配器和接口的聚合信息。这些值在tbatch执行期间不会更改。每个系统一条记录（在tbatches之间重用）。

| 键 | 定义 | 数据类型 |
|-----|------------|----------|
| NetworkStaticID | 网络静态记录的唯一SHA-1 ID | string |
| NInterface | 网络接口总数 | int32_t |
| IFTrios | 接口详细信息的JSON数组（名称、MAC、速度） | string |

**链接ID公式：** `SHA1("network_static" + <UTC时间戳> + <主机名> + <主MAC地址>)`

---

**10) 域：主板静态信息 (MotherBoardStatic)**

主板静态信息域存储有关主板的静态信息。这些值在tbatch执行期间不会更改。每个系统一条记录（在tbatches之间重用）。

| 键 | 定义 | 数据类型 |
|-----|------------|----------|
| MotherBoardStaticID | 主板静态记录的唯一SHA-1 ID | string |
| Manufacturer | 主板制造商名称 | string |
| Model | 主板型号 | string |
| BIOSVersion | BIOS/UEFI固件版本 | string |
| BIOSDateUTC | ISO 8601格式的BIOS发布日期 | string |

**链接ID公式：** `SHA1("motherboard_static" + <UTC时间戳> + <主机名> + <制造商> + <型号>)`

---

**11) 域：机箱静态信息 (ChassisStatic)**

机箱静态信息域存储有关系统机箱/外壳的静态信息。这些值在tbatch执行期间不会更改。每个系统一条记录（在tbatches之间重用）。

| 键 | 定义 | 数据类型 |
|-----|------------|----------|
| ChassisStaticID | 机箱静态记录的唯一SHA-1 ID | string |
| Type | 机箱类型（Tower/Rack/Blade/Laptop/Desktop/Cluster） | string |
| Manufacturer | 机箱制造商名称 | string |
| Model | 机箱型号描述 | string |
| CoolType | 冷却系统类型 | string |

**链接ID公式：** `SHA1("chassis_static" + <UTC时间戳> + <主机名> + <制造商> + <型号>)`

### 2.2. 记录后端：`rawdb`

本节说明用于构建TPBench数据库和支持数据库CRUD操作的集成`rawdb`后端设计。每个域在`${TPB_WORKSPACE}/rawdb/`下有自己的子目录。

#### 2.2.1 任务批次记录 (Task Batch Record)

Task Batch记录单次`tpbcli`调用或kernel执行批次的上下文信息。

**1) 头部结构 (Header Structure)**

```c
// 256字节任务批次头部（版本1.0）
typedef struct tbatch_header {
    unsigned char tbatch_id[20];        /**< TBatchID - 主链接ID（20字节SHA-1） */
    unsigned char dup_to[20];           /**< 重复跟踪：0=无，否则指向其他TBatchID */
    tpb_dtbits_t start_utc_bits;        /**< 批次开始日期时间（64位紧凑编码） */
    uint64_t start_btime_ns;            /**< 批次启动时的启动时间（自启动以来的纳秒数） */
    uint64_t duration_ns;               /**< 批次总持续时间（纳秒） */
    char hostname[64];                  /**< 执行主机名 */
    char username[64];                  /**< 启动批次的用户名 */
    uint32_t front_pid;                 /**< 前端进程ID */
    uint32_t ntask_records;             /**< 此批次中的任务记录数 */
    uint32_t nscore_records;            /**< 此批次中的评分记录数 */
    unsigned char reserve[92];          /**< 填充至256字节总计 */
} tbatch_header_t;
```

**2) Magic签名（类型指示器）**

Magic使用'TPB'后的字节作为记录类型指示器：
- **0xe0** = 任务批次记录 (TBatch Record)
- **0xe1** = 任务记录 (Task Record)  
- **0xe3** = Kernel记录 (Kernel Record)

| Magic类型 | 格式                                   | 十六进制模式                | 用途            |
| ---------- | ---------------------------------------- | -------------------------- | ------------------ |
| meta_magic | 0xe1 'T' 'P' 'B' 0x?? 'S' 0x30 0x31 0xe0 | E1 54 50 42 ?? 53 30 31 E0 | 头部部分开始   |
| data_magic | 0xe1 'T' 'P' 'B' 0x?? 'D' 0x30 0x31 0xe0 | E1 54 50 42 ?? 44 30 31 E0 | 原始数据部分开始 |
| end_magic  | 0xe1 'T' 'P' 'B' 0x?? 'E' 0x30 0x31 0xe0 | E1 54 50 42 ?? 45 30 31 E0 | 文件结束标记             |

**具体Magic值（TBatch的类型=0xe0）：**
- meta_magic: `E1 54 50 42 E0 53 30 31 E0`
- data_magic: `E1 54 50 42 E0 44 30 31 E0`
- end_magic: `E1 54 50 42 E0 45 30 31 E0`

**3) 任务批次文件布局 (`<TBatchID>.bin`)**

新布局：metasize包含meta_magic+meta部分，datasize包含data_magic+所有数据+end_magic

```
+------------------------+  <- 字节0
| meta_magic (8B)        |  <- 0xe1 'T' 'P' 'B' 0xe0 'S' 0x30 0x31 0xe0
+------------------------+  <- 字节8
| metasize (8B)          |  <- (meta_magic + meta部分)的字节数
+------------------------+  <- 字节16
| datasize (8B)          |  <- (data_magic + 所有数据 + end_magic)的字节数
+------------------------+  <- 字节24
| ntask_records (4B)     |  <- 任务记录数
+------------------------+  <- 字节28
| nscore_records (4B)    |  <- 评分记录数
+------------------------+  <- 字节32
| nheaders (4B)          |  <- 头部块数(ntask + nscore)
+------------------------+  <- 字节36
| reserve (4B)           |  <- 填充至8字节对齐
+------------------------+  <- 字节40
| header[0] (1328B)    |  <- 第一头部块（任务或评分）
| header[1] (1328B)      |  <- 第二头部块
| ...                    |  <- 总计：nheaders * 1328字节
+------------------------+  <- Meta部分结束
| tbatch_header_t (256B) |  <- 带嵌入header[]的256字节固定头部
+------------------------+  <- 变量（如果ID在头部块中）
| data_magic (8B)        |  <- 0xe1 'T' 'P' 'B' 0xe0 'D' 0x30 0x31 0xe0
+------------------------+
| all_dim_data[]         |  <- 每头部块的原始数据数组
+------------------------+
| end_magic (8B)         |  <- 0xe1 'T' 'P' 'B' 0xe0 'E' 0x30 0x31 0xe0
+------------------------+
```

**注意**: ID数组（任务记录ID和评分记录ID）现在作为头部块存储在meta部分内，而不是作为固定头部后的原始字节。

**4) 任务批次域头部文件 (`TaskBatch.binh`)**

追加模式：每个新记录创建时，直接向`.binh`文件追加完整的`tbatch_header_t`（256字节）。

**`.binh` 文件布局：**

域头部（`.binh`）使用类型指示器**0xED**（目录）：

```
+------------------------+  <- 字节0
| magic (8B)             |  <- 0xe1 'T' 'P' 'B' 0xED 'D' 'I' 'R' 0x00
+------------------------+
| version (4B)           |  <- 0x00010000（版本1.0）
+------------------------+
| reserve (4B)           |  <- 填充
+------------------------+  <- 字节16（条目开始）
| tbatch_header_t [0]    |  <- 第一任务批次记录（256字节）
| tbatch_header_t [1]   |  <- 第二条目（256字节）
| ...                    |  <- 顺序追加
+------------------------+  <- 文件结束
```

读取时顺序扫描所有256字节条目，匹配`tbatch_id`。

**5) 静态键**

| 键              | 数据类型          | 描述                                     |
| ---------------- | ----------------- | ----------------------------------------------- |
| TBatchID         | string (20B原始)  | 任务批次SHA-1链接ID                        |
| TaskRecordIDn    | list              | 任务记录ID列表(ntask_records项)   |
| ScoreRecordIDs   | list              | 评分记录ID列表(nscore_records项) |
| StartTimeUTC     | string (ISO 8601) | 人类可读的开始时间                       |
| StartMachineTime | uint64_t          | 开始时的启动时间纳秒数                  |
| Duration         | uint64_t          | 批次总持续时间（纳秒）             |
| Hostname         | string            | 执行主机（最多63字符+空字符）            |
| User             | string            | 执行用户（最多63字符+空字符）            |

**6) 链接ID公式**

```
SHA1("tbatch" + <UTC时间戳> + <机器启动纳秒数> + <主机名> + <用户名> + <持续时间纳秒数> + <前端进程ID>)
```

示例：`SHA1("tbatch20250308T130801Z3600000000000node01testuser12567890123413249")`

---

#### 2.2.2 任务记录 (Task Record)

任务记录存储单次kernel调用的输入参数和输出指标。

**1) 头部结构 (Header Structure)**

```c
// 128字节任务记录头部（版本1.0）
typedef struct task_record {
    unsigned char task_record_id[20];   /**< 任务记录ID - 主链接ID（20字节SHA-1） */
    unsigned char dup_to[20];           /**< 重复跟踪：0=无，否则指向其他任务记录ID */
    unsigned char tbatch_id[20];        /**< 外键：链接到产生此记录的任务批次 */
    unsigned char kernel_id[20];        /**< 外键：链接到kernel定义记录 */
    
    tpb_dtbits_t utc_bits;              /**< 调用日期时间（64位紧凑编码） */
    uint64_t btime_ns;                  /**< 调用时的启动时间（自启动以来的纳秒数） */
    uint64_t duration_ns;               /**< kernel执行持续时间（纳秒） */
    
    uint32_t exit_code;                 /**< kernel退出代码（0=成功） */
    uint32_t handle_index;              /**< 批次内的handle索引（从0开始） */
    int32_t  mpi_rank;                  /**< MPI等级（非MPI为-1，MPI启用为0-N） */
    uint32_t reserve;                   /**< 填充至8字节对齐 */
} task_record_t;
```

**字段说明：**
- `task_record_id`：此记录的主键ID（**注意**：原草案错误命名为`kernel_record_id`，已修正）
- `tbatch_id`：外键，指向所属的任务批次
- `kernel_id`：外键，指向kernel记录
- `handle_index`：标识这是批次中第几个handle的执行结果
- `mpi_rank`：MPI并行时的等级，-1表示非MPI kernel
- `duration_ns`：重命名为`duration_ns`（原`duration`），与任务批次命名保持一致

**2) Magic签名（任务的类型=0xe1）**

| Magic      | 十六进制值                      | 用途            |
| ---------- | ------------------------------ | ------------------ |
| meta_magic | `E1 54 50 42 E1 53 30 31 E0`   | 头部部分开始   |
| data_magic | `E1 54 50 42 E1 44 30 31 E0`   | 原始数据部分开始 |
| end_magic  | `E1 54 50 42 E1 45 30 31 E0`   | 文件结束标记             |

**3) 任务记录文件布局 (`<TaskRecordID>.bin`)**

```
+------------------------+  <- 字节0
| meta_magic (8B)        |  <- 0xe1 'T' 'P' 'B' 0xe1 'S' 0x30 0x31 0xe0
+------------------------+  <- 字节8
| metasize (8B)          |  <- (meta_magic + meta部分)的字节数
+------------------------+  <- 字节16
| datasize (8B)          |  <- (data_magic + 所有数据 + end_magic)的字节数
+------------------------+  <- 字节24
| task_record_t (128B)   |  <- 带任务信息的固定头部
+------------------------+  <- 字节152
| header[0] (1328B)      |  <- 第一头部块（输入/输出定义）
| header[1] (1328B)      |  <- 第二头部块
| ...                    |  <- 动态元数据块
+------------------------+  <- Meta部分结束
| data_magic (8B)        |  <- 0xe1 'T' 'P' 'B' 0xe1 'D' 0x30 0x31 0xe0
+------------------------+
| all_dim_data[]         |  <- 每头部块的原始数据数组
+------------------------+
| end_magic (8B)         |  <- 0xe1 'T' 'P' 'B' 0xe1 'E' 0x30 0x31 0xe0
+------------------------+
```

**4) 任务记录域头部文件 (`TaskRecord.binh`)**

追加模式：每个新记录创建时，直接向`.binh`文件追加完整的`task_record_t`（128字节）。

**`.binh` 文件布局：**

```
+------------------------+  <- 字节0
| magic (8B)             |  <- 0xe1 'T' 'P' 'B' 0xED 'D' 'I' 'R' 0x00
+------------------------+
| version (4B)           |  <- 0x00010000（版本1.0）
+------------------------+
| reserve (4B)           |  <- 填充
+------------------------+  <- 字节16（条目开始）
| task_record_t [0]      |  <- 第一任务记录（128字节）
| task_record_t [1]      |  <- 第二条目（128字节）
| ...                    |  <- 顺序追加
+------------------------+  <- 文件结束
```

读取时顺序扫描所有128字节条目，匹配`task_record_id`。

**关于`utc_bits`编码的说明：**

`tpb_dtbits_t` (uint64_t)将日期时间和时区打包成紧凑的64位表示：

| 位范围 | 字段 | 大小 | 范围 |
|-----------|-------|------|-------|
| 0-5       | 秒 | 6位 | 0-59 |
| 6-11      | 分钟 | 6位 | 0-59 |
| 12-16     | 小时   | 5位 | 0-23 |
| 17-21     | 天     | 5位 | 1-31 |
| 22-25     | 月   | 4位 | 1-12 |
| 26-33     | 年偏差 | 8位 | 0-255 (1970-2225) |
| 34-41     | 时区 | 8位 | 15分钟增量 |
| 42-63     | 保留 | 22位 | 未使用 |

**年编码：** 年存储为从1970开始的偏差（例如，2026年存储为56）。

**时区编码：** 时区偏差以15分钟增量存储。该值是有符号的（二进制补码），范围约为-32到+31.75小时。

**字节序说明：**
位布局由位位置（0-63）定义，与主机字节序无关。字段访问通过位移和掩码操作执行，在大端和小端系统上产生相同的结果：

```c
/* 示例：提取秒（位0-5） */
uint8_t sec = (uint8_t)((utc_bits >> 0) & 0x3F);  /* 在任何字节序上工作 */

/* 示例：提取年偏差（位26-33） */
uint8_t year_bias = (uint8_t)((utc_bits >> 26) & 0xFF);
uint16_t year = 1970 + year_bias;
```

当`utc_bits`存储到磁盘或通过网络传输时，它存储为本机`uint64_t`值。读取器应将其视为不透明的64位整数，并使用上面显示的相同位提取操作。不要直接将内存转换为字节数组或假设特定的字节序。

**`btime_ns`与日期时间：**
`btime_ns`字段存储高精度启动时间（自系统启动以来的纳秒数），与日历日期时间无关。对于从启动时间秒到日历日期时间的粗略转换：
- 每天24小时
- 每月730小时（365/12 * 24）
- 每年8760小时（365 * 24）

**5) 静态键**

| 键              | 数据类型          | 描述                         |
| ---------------- | ----------------- | ----------------------------------- |
| TaskRecordID     | string (20B原始)  | 任务记录的SHA-1 ID              |
| TBatchID         | string (20B原始)  | 外键到任务批次           |
| KernelID         | string (20B原始)  | 外键到kernel定义    |
| StartTimeUTC     | string (ISO 8601) | 调用开始时间               |
| StartMachineTime | uint64_t          | 调用时的启动时间纳秒数 |
| Duration         | uint64_t          | 执行持续时间（纳秒）   |
| ExitCode         | int32_t           | kernel退出代码                    |
| HandleIndex      | uint32_t          | 批次内的handle位置        |
| MPIRank          | int32_t           | MPI等级或-1                      |

**6) 链接ID公式**

```
SHA1("task" + <UTC时间戳> + <机器启动纳秒数> + <持续时间> + <主机名> + <用户名> + <kernel名称> + <进程ID> + <批次中的顺序>)
```

---

#### 2.2.3 Kernel记录 (Kernel Record)

Kernel记录存储被评估kernel的元数据，包括定义、参数、度量指标。

**1) 头部结构 (Header Structure)**

```c
// 856字节kernel记录头部（版本1.0）
#define TPBM_NAME_STR_MAX_LEN 256
#define TPBM_NOTE_STR_MAX_LEN 2048

typedef struct kernel_record {
    unsigned char kernel_id[20];        /**< KernelID - 主链接ID（20字节SHA-1） */
    unsigned char dup_to[20];           /**< 重复跟踪：0=无，否则指向其他KernelID */
    
    char kernel_name[TPBM_NAME_STR_MAX_LEN];      /**< kernel名称（不含tpbk_前缀） */
    char version[TPBM_NAME_STR_MAX_LEN];          /**< 版本字符串 */
    char description[TPBM_NOTE_STR_MAX_LEN];       /**< 人类可读描述 */
    
    tpb_dtbits_t compile_utc_bits;      /**< 编译日期时间（64位紧凑编码） */
    unsigned char source_file_hash[32]; /**< 源文件的SHA-256哈希 */
    
    uint32_t nparms;                    /**< kernel定义中的参数数量 */
    uint32_t nouts;                     /**< kernel定义中的输出指标数量 */
    uint32_t kctrl;                     /**< kernel控制位（FLI=1, PLI=2, ALI=4） */
    uint32_t reserve;                   /**< 填充至8字节对齐 */
} kernel_record_t;
```

**字段说明：**
- `source_file_hash[32]`：用于生成KernelID和完整性校验
- `compile_utc_bits`：编译时间（非执行时间）
- `kctrl`：复用`tpb-public.h`定义的`TPB_KTYPE_FLI/PLI/ALI`
- `nparms`, `nouts`：来自kernel注册的参数和输出定义数量

**2) Magic签名（Kernel的类型=0xe3）**

| Magic      | 十六进制值                      | 用途            |
| ---------- | ------------------------------ | ------------------ |
| meta_magic | `E1 54 50 42 E3 53 30 31 E0`   | 头部部分开始   |
| data_magic | `E1 54 50 42 E3 44 30 31 E0`   | 原始数据部分开始 |
| end_magic  | `E1 54 50 42 E3 45 30 31 E0`   | 文件结束标记             |

**3) Kernel记录文件布局 (`<KernelID>.bin`)**

```
+------------------------+  <- 字节0
| meta_magic (8B)        |  <- 0xe1 'T' 'P' 'B' 0xe3 'S' 0x30 0x31 0xe0
+------------------------+  <- 字节8
| metasize (8B)          |  <- (meta_magic + meta部分)的字节数
+------------------------+  <- 字节16
| datasize (8B)          |  <- (data_magic + 所有数据 + end_magic)的字节数
+------------------------+  <- 字节24
| kernel_record_t (856B)   |  <- 带kernel信息的固定头部
+------------------------+  <- 字节880
| header[0] (1328B)      |  <- 第一头部块（参数/度量定义）
| header[1] (1328B)      |  <- 第二头部块
| ...                    |  <- 动态元数据块
+------------------------+  <- Meta部分结束
| data_magic (8B)        |  <- 0xe1 'T' 'P' 'B' 0xe3 'D' 0x30 0x31 0xe0
+------------------------+
| all_dim_data[]         |  <- 每头部块的原始数据数组
+------------------------+
| end_magic (8B)         |  <- 0xe1 'T' 'P' 'B' 0xe3 'E' 0x30 0x31 0xe0
+------------------------+
```

**4) Kernel域头部文件 (`Kernel.binh`)**

追加模式：每个新记录创建时，直接向`.binh`文件追加完整的`kernel_record_t`（856字节）。

**`.binh` 文件布局：**

```
+------------------------+  <- 字节0
| magic (8B)             |  <- 0xe1 'T' 'P' 'B' 0xED 'D' 'I' 'R' 0x00
+------------------------+
| version (4B)           |  <- 0x00010000（版本1.0）
+------------------------+
| reserve (4B)           |  <- 填充
+------------------------+  <- 字节16（条目开始）
| kernel_record_t [0]    |  <- 第一kernel记录（856字节）
| kernel_record_t [1]    |  <- 第二条目（856字节）
| ...                    |  <- 顺序追加
+------------------------+  <- 文件结束
```

读取时顺序扫描所有856字节条目，匹配`kernel_id`。

**5) 静态键**

| 键                  | 数据类型          | 描述                                   |
| -------------------- | ----------------- | --------------------------------------------- |
| KernelID             | string (20B原始)  | kernel SHA-1 ID                               |
| KernelName           | string            | kernel名称（最多255字符）                   |
| Version              | string            | 版本字符串（最多255字符）                |
| Description          | string            | 人类可读描述（最多2047字符）   |
| SourceFile           | string            | 源文件路径（在元数据中）             |
| CompileDateUTC       | string (ISO 8601) | 编译时间戳（来自compile_utc_bits） |
| ParameterDefinitions | JSON string       | 参数定义数组                   |
| MetricDefinitions    | JSON string       | 输出指标定义数组              |

**6) 链接ID公式**

```
SHA1("kernel_def" + <kernel名称> + <版本> + <源文件哈希>)
```

---

### 2.2.4 文件组织总结（追加模式 + Magic类型指示器）

**Magic类型指示器总结：**

| 类型          | 指示器 | 'TPB'后的字节         | 用途                     |
| ------------- | --------- | ------------------------ | ------------------------- |
| 任务批次记录 | 0xe0      | `E1 54 50 42 **E0** ...` | `<TBatchID>.bin`文件       |
| 任务记录   | 0xe1      | `E1 54 50 42 **E1** ...` | `<TaskRecordID>.bin`文件   |
| kernel记录 | 0xe3      | `E1 54 50 42 **E3** ...` | `<KernelID>.bin`文件       |
| 域头部 | 0xED      | `E1 54 50 42 **ED** ...` | `.binh`目录文件            |

**目录结构：**

```
${TPB_WORKSPACE}/rawdb/
├── TaskBatch/
│   ├── TaskBatch.binh          # 16字节头部(magic=0xED) + N*256字节条目
│   ├── <TBatchID>.bin          # meta_magic + metasize + datasize + ntask + nscore + 
│   │                           # nheaders + header[] + tbatch_header_t + data_magic + 
│   │                           # all_dim_data + end_magic
│   └── ...
├── Kernel/
│   ├── Kernel.binh             # 16字节头部(magic=0xED) + N*856字节条目
│   ├── <KernelID>.bin          # meta_magic + metasize + datasize + kernel_record_t + 
│   │                           # header[] + data_magic + all_dim_data + end_magic
│   └── ...
├── TaskRecord/
│   ├── TaskRecord.binh         # 16字节头部(magic=0xED) + N*128字节条目
│   ├── <TaskRecordID>.bin      # meta_magic + metasize + datasize + task_record_t + 
│   │                           # header[] + data_magic + all_dim_data + end_magic
│   └── ...
└── ... (其他域)
```

**`.binh` 追加模式说明：**
- 创建时写入16字节文件头（magic=0xED + version + reserve）
- 每新增一个记录，向文件尾追加固定大小的条目（任务批次=256B, kernel=856B, 任务记录=128B）
- 读取时从偏移16开始顺序扫描所有条目
- 通过匹配ID字段查找目标记录
- 通过头部中的信息定位到实际的`.bin`文件

**.bin 文件特点：**
- 以`meta_magic`开始（类型特定: 0xe0/e1/e3）
- 包含metasize（meta_magic到meta部分结束的字节数）
- 包含datasize（data_magic到end_magic的字节数）
- 包含头部块（原元数据块，1328字节/块）
- 包含`data_magic`分隔符
- 以`end_magic`结束（类型特定）
- 可嵌入任意文件格式，通过magic识别边界

**记录尺寸汇总：**

| 记录类型   | 头部大小 | .binh条目大小 | Magic |
| ------------- | ----------- | ------------------ | ----- |
| 任务批次     | 256字节   | 256字节          | 0xe0  |
| 任务记录    | 128字节   | 128字节          | 0xe1  |
| kernel记录  | 856字节   | 856字节          | 0xe3  |
| 域头部  | 16字节    | 不适用                | 0xED  |

---

## 3. 记录前端 (Record Frontend)

### 3.1. 列出记录 (List records)

列出最新的20条kernel或评分记录，每行一条记录。
```bash
### 以下命令相等。
$ tpbcli record list
$ tpbcli record ls
$ tpbcli r list
$ tpbcli r ls
### --- 输出 ---
TPBench v1.0
2024-01-01T08:11:30 [I] TPB_HOME: /usr/local/tpbench
2024-01-01T08:11:30 [I] TPB_WORKSPACE: /usr/local/tpbench
2024-01-01T08:11:30 [I] 从/usr/local/tpbench/.tpb_data读取记录 ... 完成
2024-01-01T08:11:30 [I] 最新20条记录列表
===
批次时间,记录时间,持续时间,参数数,编号
===
2024-01-01T08:11:30 [I] TPBench退出。

```
