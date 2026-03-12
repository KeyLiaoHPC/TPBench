# The Data Record Component Design

## 1. Introduction

### 1.1. Rationale and Architecture

TPBench records data that influences kernel test outputs and captures results produced by kernel invocations. Due to the nature that actual performance is highly sensitive to program context and underlying environment, TPBench tags each record with a dedicated ID for further analysis in conjunction with other kernels and environmental factors.

Target metrics are measurements resulting from a complex co.tpbration of computing hardware states, software stack configurations, and workload input arguments/data. Theoretically, one could take snapshots of the entire system along with target metrics to build a comprehensive database covering performance, power, portability, accuracy, and resilience characteristics. However, the trade-off between overhead and observation granularity in computing systems is inevitable.

Eventually, data recording frequency, technique varies between data aspects. TPBench uses multiple **data domains** to record different aspects of the computing system, such as system states, kernel definitions, input arguments, target metrics, etc. Different records have predefined recording trigger and are linked via SHA-1 IDs. This approach avoids recording complete snapshots for each test while maintaining traceability.

Each domain has fixed-name record and dynamic record. Fixed-named records are attributes used to characterize the basic information of each piece of record in a domain, and dynamic record is named by configurable record headers with real-time record data:
- **Attribute**: Static attributes characterzing an instance in a domain, recorded in designated byte positions with predefined fixed names, definitions, format and parsers.
- **Header**: Headers are used to define customizable structures to store run-time dynamic data. A header is the description section providing information of the record data it holds. (e.g.dimensions, names， data type)
- **Record Data**: Data recorded by TPBench.

In the **rawdb** backend, the **attributes**, **headers** and **record data** are stored in the TPBench entry files and TPBench record files:
- **TPBench Entry File** (`<DomainName>.tpbe`): Recording attributes of each single record in the workspace, starting with a 8-Byte TPBench magic signature as a notation to the file type and the domain. All attribute values in the data is stored posisional with fixed data size. Each new record appends a set of attrubutes as an entry to the record file.
- **TPBench Record File** (`<RecordID>.tpbr`): Dynamic record data, constructing by a metadata section, and data sections. Data in the record file is self explained and self pointed. Each new record generate a new record file.

Each domain records a specific aspect of system status that characterizes the states, inputs, and outputs of task batches and kernel invocations. However, recording all domains at every kernel invocation leads to significant overhead. Therefore, settings in `${TPB_WORKSPACE}/etc/config.json` can control recording behavior:
- `auto_collect`: Boolean flag to enable/disable automatic collection for a domain
- `action`: Trigger condition for recording (e.g., `"kernel_invoke"`, `"user_invoke"`)

### 1.2. Concepts

**Task Batch (TBatch)**: A task batch (tbatch) is the execution context initiated by a single TPBench front-end invocation (e.g., `tpbcli run`). A tbatch may execute one or more kernel invocations, each with its own input arguments. All kernel invocations within a tbatch share a common TBatchID, which tags all records produced during that execution. For valid test results, a tbatch requires stable run-time environment matching the actual scenario. Specifically:
- If someone uses a script to invoke multiple `tpbcli` commands sequentially, each invocation should be treated as a separate tbatch.
- If underlying hardware or software configurations change between invocations, each configuration state constitutes a separate tbatch. (Please don't trouble yourself.)

**TBatchID**: A unique identifier for a task batch, generated as:
```
SHA1("tbatch" + <UTC_timestamp> + <machine_start_nanoseconds> + <hostname> + <username> + <front_end_pid>)
```
Where `<machine_start_nanoseconds>` is the number of nanoseconds since system boot (uint64_t).
Example: `SHA1("tbatch20250308T130801Z3600000000000node01testuser13249")`

**Kernel**: A kernel is a program or code module that users wish to evaluate. Kernels reside in `${TPB_HOME}/lib` or `${TPB_WORKSPACE}/lib/` and are named `tpbk_<kernel_name>.<so|x>`.

---

**Workload**: A workload is the concrete instantiation of a kernel for benchmarking, defined by:
- The kernel name (identifying the code to execute)
- Input parameters (values assigned to the kernel's parameter definitions)
- Data size (problem scale, e.g., array length, memory size)

Workload represents the *configurable* aspects of a benchmark execution. Hardware state, OS configuration, and other environmental factors are *not* part of the workload definition but are recorded separately as execution context.

---

**Parameter**: A parameter is a named, typed placeholder defined by a kernel, representing a configurable factor that may affect measurement results. Parameters have:
- A name (e.g., `ntest`, `total_memsize`)
- A data type (int, double, string, etc.)
- A default value
- Optional validation constraints (range, list, custom)

---

**Argument**: An argument is the concrete value (number, string, or data reference) supplied to a parameter during workload execution. Arguments are provided via CLI options (`--kargs`, `--kenvs`, `--kmpiargs`) or configuration files.

---

**Target Metric**: A target metric is a quantity that users wish to measure, observe, or derive from a workload execution. The most common metric is runtime. In TPBench, metrics may also include:
- Performance metrics (throughput, latency, FLOPS)
- Correctness, accuracy, or other arithmetic precision indicators
- Resource usage (power, memory bandwidth)
- Any user-defined observable from the workload

---

**Domain / Record**: A Record stores data that influences kernel test outputs and results produced by kernel invocations. Each Record includes a unique SHA-1 ID and related ID to link and be linked. Each record belongs to a domain, which classifies different kinds of records into different aspects, and allow different data to be accquired and recorded in different scenarios.

---

**Record Types and IDs**

| Record Type | ID | Description |
|-------------|-----------|-------------|
| **TBatch Record** | `SHA1("tbatch" + <UTC_timestamp> + <btime> + <hostname> + <username> + <front_end_pid>)` | One record per tbatch invocation |
| **Kernel Record** | `SHA1("kernel" + <kernel_name> + <library_sha256> + .tpbrary_sha256>)` | Check or generate once a kernel is built. |
| **Task Record** | `SHA1("task" + <UTC_timestamp> + <btime> + <hostname> + <username> + <kernel_name> + <kernel_pid> + <order_in_batch>)` | One record per kernel invocation |
| **Score Record** | `SHA1("score" + <UTC_timestamp> + <btime> + <hostname> + <username> + <score_name> + <calc_order>)` | One record per calculated score or sub-score |

## 2. The Design of Data Record Logic in TPBench Corelib

### 2.1. Logical Organization of Database

TPBench's records are stored and managed under a workspace, except for special cross-workspace tools. TPBench uses two environment variables to navigate the filesystem: `TPB_HOME` is the TPBench installation path, and `TPB_WORKSPACE` is the root folder of a TPBench project.

**1) Workspace Resolution:**
1. If `${TPB_WORKSPACE}` environment variable is explicitly set, use that directory as the workspace.
2. Otherwise, search `$TPB_HOME/.tpb_workspace/` for workspace configuration files.
3. If not found there, search `$HOME/.tpb_workspace/`.

For more information related to the build system and filesystem structure, refer to [design_build.md](./design_build.md).

**2) File Structure**

TPBench implement a integrated `rawdb` backend for data management and operations. More backend (e.g. SQLite, HDF5) will be added in future.

In `rawdb`, each record domain has two kinds of file, the TPBench header (.tpbe) and the TPBench record (.tpbr). `.tpbh` is the incremental header that chases all record of the domain in current workspace, and `.tpbr` stores parts of attributes, headers, and detail record data.

File tree:
```
${TPB_WORKSPACE}/rawdb/
├── TaskBatch/
│   ├── TaskBatch.tpbe 
│   ├── <TBatchID_0>.tpbr
│   ├── <TBatchID_1>.tpbr
│   └── ...
├── Kernel/
│   ├── Kernel.tpbe
│   ├── <KernelID_0>.tpbr
│   ├── <KernelID_1>.tpbr
│   └── ...
├── TaskRecord/
│   ├── TaskRecord.tpbe
│   ├── <TaskRecordID_0>.tpbr
│   ├── <TaskRecordID_1>.tpbr
│   └── ...
└── ... 
```

### 2.2. Task Batch Record

A task batch (tbatch) is the execution context that invokes one or more kernels. Each `tpbcli` call or dedicated kernel execution (e.g., directly invoking `tpbk_<stream>.x`) creates a new tbatch. TBatchID serves as the Link ID for each tbatch record.

**1) Entry Structure**

File structure:
```
+------------------------+  <- Byte 0
| meta_magic (8B)        |  <- 0xe1 'T' 'P' 'B' 0xd0 'S' 0x31 0xe0
+------------------------+  <- Byte 8
| entry[0] (256B)        |
+------------------------+
| ...                    |
+------------------------+
| entry[N-1] (256B)      |
+------------------------+
| end_magic (8B)         |  <- 0xe1 'T' 'P' 'B' 0xd0 'E' 0x31 0xe0
+------------------------+
```



Entry member:

```c
// 256-Byte task-batch header (version 1.0)
typedef struct tbatch_attr {
    unsigned char tbatch_id[20];        /**< TBatchID - Primary Link ID (20-byte SHA-1) */
    unsigned char dup_to[20];           /**< Duplicate tracking: 0=none, else points to other TBatchID */
    tpb_dtbits_t start_utc_bits;        /**< Batch start datetime (64-bit compact encoding) */
    uint64_t start_btime_ns;            /**< Boot time at batch start (nanoseconds since boot) */
    uint64_t duration_ns;               /**< Total batch duration in nanoseconds */
    char hostname[64];                  /**< Execution host name */
    char username[64];                  /**< Username who initiated batch */
    uint32_t front_pid;                 /**< Front-end process ID */
    uint32_t ntask_records;             /**< Number of task records in this batch */
    uint32_t nscore_records;            /**< Number of score records in this batch */
    unsigned char reserve[92];          /**< Padding to 256 bytes total */
} tbatch_attr_t;
```

TBatchID:

```
SHA1("tbatch" + <UTC_timestamp> + <machine_start_nanoseconds> + <hostname> + <username> + <front_end_pid>)
```

Example: `SHA1("tbatch20250308T130801Z3600000000000node01testuser13249")`

Magic signature:

| Magic      | Text |Hex Value                      | Purpose              |
| ---------- | ---  |------------------------------ | -------------------- |
| entry_begin_magic | . T P B . S 1 . |`E1 54 50 42 D0 53 31 E0`   | Entry file begin |
| entry_end_magic | . T P B . E 1 . |`E1 54 50 42 D0 45 31 E0`   | Entry file end |

**2) Record Structure**

| Magic      | Hex Value                      | Purpose              |
| ---------- | ------------------------------ | -------------------- |
| meta_magic | `E1 54 50 42 E0 53 31 E0`   | Meta section         |
| data_magic | `E1 54 50 42 E0 44 31 E0`   | Record data section  |
| end_magic  | `E1 54 50 42 E0 45 31 E0`   | End of tpbr          | 

**3) Task Batch File Layout (`<TBatchID>.tpbr`)**

新布局：metasize包含meta_magic+meta section，datasize包含data_magic+all data+end_magic

```
+------------------------+  <- Byte 0
| meta_magic (8B)        |  <- 0xe1 'T' 'P' 'B' 0xe0 'S' 0x30 0x31 0xe0
+------------------------+  <- Byte 8
| metasize (8B)          |  <- Bytes of (meta_magic + meta section)
+------------------------+  <- Byte 16
| datasize (8B)          |  <- Bytes of (data_magic + all data + end_magic)
+------------------------+  <- Byte 24
| ntask_records (4B)     |  <- Number of task records
+------------------------+  <- Byte 28
| nscore_records (4B)    |  <- Number of score records
+------------------------+  <- Byte 32
| nheaders (4B)          |  <- Number of header blocks (ntask + nscore)
+------------------------+  <- Byte 36
| reserve (4B)           |  <- Padding to 8-byte alignment
+------------------------+  <- Byte 40
| header[0] (1328B)    |  <- First header block (task or score)
| header[1] (1328B)      |  <- Second header block
| ...                    |  <- Total: nheaders * 1328 bytes
+------------------------+  <- End of meta section
| tbatch_header_t (256B) |  <- 256-byte fixed header with IDs embedded in header[]
+------------------------+  <- Variable (if IDs are in header blocks)
| data_magic (8B)        |  <- 0xe1 'T' 'P' 'B' 0xe0 'D' 0x30 0x31 0xe0
+------------------------+
| all_dim_data[]         |  <- Raw data arrays per header block
+------------------------+
| end_magic (8B)         |  <- 0xe1 'T' 'P' 'B' 0xe0 'E' 0x30 0x31 0xe0
+------------------------+
```

**Note**: ID arrays (TaskRecordIDs and ScoreRecordIDs) are now stored as header blocks within the meta section, not as raw bytes after the fixed header.

**4) Task Batch Domain Header (`TaskBatch.tpbe`)**

追加模式：每个新记录创建时，直接向`.tpbe`文件追加完整的`tbatch_header_t`（256字节）。

**`.tpbe` File Layout：**

Domain header (`.tpbe`) uses type indicator **0xED** (Directory):

```
+------------------------+  <- Byte 0
| magic (8B)             |  <- 0xe1 'T' 'P' 'B' 0xED 'D' 'I' 'R' 0x00
+------------------------+
| version (4B)           |  <- 0x00010000 (version 1.0)
+------------------------+
| reserve (4B)           |  <- Padding
+------------------------+  <- Byte 16 (start of entries)
| tbatch_header_t [0]    |  <- First task batch record (256 bytes)
| tbatch_header_t [1]   |  <- Second entry (256 bytes)
| ...                    |  <- Appended sequentially
+------------------------+  <- End of file
```

读取时顺序扫描所有256-byte entry，匹配`tbatch_id`。

**6) Link ID Formula**




### 2.3. Kernel Record

The Kernel domain stores metadata about a kernel being evaluated, including description, version, parameter definitions, and metric definitions. One record per unique kernel (shared across invocations).

| Key | Definition | DataType |
|-----|------------|----------|
| KernelID | Kernel domain's unique identifier | string |
| KernelName | Name of the kernel (without `tpbk_` prefix) | string |
| Version | Kernel version string | string |
| Description | Human-readable description of the kernel's purpose | string |
| SourceFile | Path to source file used for compilation | string |
| CompileDateUTC | Compilation timestamp in ISO 8601 format | string |
| ParameterDefinitions | JSON array descr.tpbrg input parameters | string |
| MetricDefinitions | JSON array defining measurable outputs | string |

**Link ID Formula:** `SHA1("kernel" + <kernel_name> + <library_sha256> + .tpbrary_sha256>)`


Kernel Record存储被评估kernel的元数据，包括定义、参数、度量指标。

**1) Header Structure**

```c
// 856-Byte kernel-record header (version 1.0)
#define TPBM_NAME_STR_MAX_LEN 256
#define TPBM_NOTE_STR_MAX_LEN 2048

typedef struct kernel_record {
    unsigned char kernel_id[20];        /**< KernelID - Primary Link ID (20-byte SHA-1) */
    unsigned char dup_to[20];           /**< Duplicate tracking: 0=none, else points to other KernelID */
    
    char kernel_name[TPBM_NAME_STR_MAX_LEN];      /**< Kernel name (without tpbk_ prefix) */
    char version[TPBM_NAME_STR_MAX_LEN];          /**< Version string */
    char description[TPBM_NOTE_STR_MAX_LEN];       /**< Human-readable description */
    
    tpb_dtbits_t compile_utc_bits;      /**< Compilation datetime (64-bit compact encoding) */
    unsigned char source_file_hash[32]; /**< SHA-256 hash of source file */
    
    uint32_t nparms;                    /**< Number of parameters from kernel definition */
    uint32_t nouts;                     /**< Number of output metrics from kernel definition */
    uint32_t kctrl;                     /**< Kernel control bits (FLI=1, PLI=2, ALI=4) */
    uint32_t reserve;                   /**< Padding for 8-byte alignment */
} kernel_record_t;
```

**字段说明：**
- `source_file_hash[32]`：用于生成KernelID和完整性校验
- `compile_utc_bits`：编译时间（非执行时间）
- `kctrl`：复用`tpb-public.h`定义的`TPB_KTYPE_FLI/PLI/ALI`
- `nparms`, `nouts`：来自kernel注册的参数和输出定义数量

**2) Magic Signatures**

| Magic      | Hex Value                   | Purpose              |
| ---------- | --------------------------- | -------------------- |
| meta_magic | `E1 54 50 42 E1 53 31 E0`   | Meta section         |
| data_magic | `E1 54 50 42 E1 44 31 E0`   | Record data section  |
| end_magic  | `E1 54 50 42 E1 45 31 E0`   | End of tpbr          | 

**3) Kernel Record File Layout (`<KernelID>.tpbr`)**

```
+------------------------+  <- Byte 0
| meta_magic (8B)        |  <- 0xe1 'T' 'P' 'B' 0xe3 'S' 0x30 0x31 0xe0
+------------------------+  <- Byte 8
| metasize (8B)          |  <- Bytes of (meta_magic + meta section)
+------------------------+  <- Byte 16
| datasize (8B)          |  <- Bytes of (data_magic + all data + end_magic)
+------------------------+  <- Byte 24
| kernel_record_t (856B)   |  <- Fixed header with kernel info
+------------------------+  <- Byte 880
| header[0] (1328B)      |  <- First header block (parameter/metric definition)
| header[1] (1328B)      |  <- Second header block
| ...                    |  <- Dynamic metadata blocks
+------------------------+  <- End of meta section
| data_magic (8B)        |  <- 0xe1 'T' 'P' 'B' 0xe3 'D' 0x30 0x31 0xe0
+------------------------+
| all_dim_data[]         |  <- Raw data arrays per header block
+------------------------+
| end_magic (8B)         |  <- 0xe1 'T' 'P' 'B' 0xe3 'E' 0x30 0x31 0xe0
+------------------------+
```

**4) Kernel Domain Header (`Kernel.tpbe`)**

追加模式：每个新记录创建时，直接向`.tpbe`文件追加完整的`kernel_record_t`（856字节）。

**`.tpbe` File Layout：**

```
+------------------------+  <- Byte 0
| magic (8B)             |  <- 0xe1 'T' 'P' 'B' 0xED 'D' 'I' 'R' 0x00
+------------------------+
| version (4B)           |  <- 0x00010000 (version 1.0)
+------------------------+
| reserve (4B)           |  <- Padding
+------------------------+  <- Byte 16 (start of entries)
| kernel_record_t [0]    |  <- First kernel record (856 bytes)
| kernel_record_t [1]    |  <- Second entry (856 bytes)
| ...                    |  <- Appended sequentially
+------------------------+  <- End of file
```

读取时顺序扫描所有856-byte entry，匹配`kernel_id`。

**5) Static Keys**

| Key                  | DataType          | Description                                   |
| -------------------- | ----------------- | --------------------------------------------- |
| KernelID             | string (20B raw)  | Kernel SHA-1 ID                               |
| KernelName           | string            | Kernel name (max 255 chars)                   |
| Version              | string            | Version string (max 255 chars)                |
| Description          | string            | Human-readable description (max 2047 chars)   |
| SourceFile           | string            | Path to source file (in metadata)             |
| CompileDateUTC       | string (ISO 8601) | Compilation timestamp (from compile_utc_bits) |
| ParameterDefinitions | JSON string       | Parameter definitions array                   |
| MetricDefinitions    | JSON string       | Output metrics definitions array              |

**6) Link ID Formula**

```
SHA1("kernel_def" + <kernel_name> + <version> + <source_file_hash>)
```

### 2.4. Task Record

The TaskRecord domain stores the input arguments and output metrics from a single kernel invocation. One record per handle execution.

| Key | Definition | DataType |
|-----|------------|----------|
| TaskRecordID | Task record's unique SHA-1 ID | string |
| TBatchID | Link to a tbatch record | string |
| KernelID | Link to a kernel record | string |
| StartTimeUTC | Invocation start time in ISO 8601 format | string |
| StartMachineTime | Start time as nanoseconds since boot | uint64_t |
| Duration | Execution duration in nanoseconds | uint64_t |
| ExitCode | Kernel exit code (0 = success) | int32_t |

**TaskRecordID Formula:** `SHA1("task" + <UTC_timestamp> + <machine_start_nanoseconds> + <duration> + <hostname> + <username> + <kernel_name> + <pid> + <order_in_batch>)`


**1) Header Structure**

```c
// 128-Byte task-record header (version 1.0)
typedef struct task_record {
    unsigned char task_record_id[20];   /**< TaskRecordID - Primary Link ID (20-byte SHA-1) */
    unsigned char dup_to[20];           /**< Duplicate tracking: 0=none, else points to other TaskRecordID */
    unsigned char tbatch_id[20];        /**< Foreign key: links to task batch that produced this record */
    unsigned char kernel_id[20];        /**< Foreign key: links to kernel definition record */
    
    tpb_dtbits_t utc_bits;              /**< Invocation datetime (64-bit compact encoding) */
    uint64_t btime_ns;                  /**< Boot time at invocation (nanoseconds since boot) */
    uint64_t duration_ns;               /**< Kernel execution duration in nanoseconds */
    
    uint32_t exit_code;                 /**< Kernel exit code (0=success) */
    uint32_t handle_index;              /**< Handle index within batch (0-based) */
    int32_t  mpi_rank;                  /**< MPI rank (-1 if non-MPI, 0-N if MPI enabled) */
    uint32_t reserve;                   /**< Padding to 8-byte alignment */
} task_record_t;
```

**字段说明：**
- `task_record_id`：本记录的主键ID（**注意**：原草案错误命名为`kernel_record_id`，已修正）
- `tbatch_id`：外键，指向所属的TaskBatch
- `kernel_id`：外键，指向Kernel记录
- `handle_index`：标识这是batch中第几个handle的执行结果
- `mpi_rank`：MPI并行时的rank，-1表示非MPI kernel
- `duration_ns`：重命名为`duration_ns`（原`duration`），与task batch命名保持一致

**2) Magic Signatures**

| Magic      | Hex Value                   | Purpose              |
| ---------- | --------------------------- | -------------------- |
| meta_magic | `E1 54 50 42 E2 53 31 E0`   | Meta section         |
| data_magic | `E1 54 50 42 E2 44 31 E0`   | Record data section  |
| end_magic  | `E1 54 50 42 E2 45 31 E0`   | End of tpbr          | 

**3) Task Record File Layout (`<TaskRecordID>.tpbr`)**

```
+------------------------+  <- Byte 0
| meta_magic (8B)        |  <- 0xe1 'T' 'P' 'B' 0xe1 'S' 0x30 0x31 0xe0
+------------------------+  <- Byte 8
| metasize (8B)          |  <- Bytes of (meta_magic + meta section)
+------------------------+  <- Byte 16
| datasize (8B)          |  <- Bytes of (data_magic + all data + end_magic)
+------------------------+  <- Byte 24
| task_record_t (128B)   |  <- Fixed header with task info
+------------------------+  <- Byte 152
| header[0] (1328B)      |  <- First header block (input/output definition)
| header[1] (1328B)      |  <- Second header block
| ...                    |  <- Dynamic metadata blocks
+------------------------+  <- End of meta section
| data_magic (8B)        |  <- 0xe1 'T' 'P' 'B' 0xe1 'D' 0x30 0x31 0xe0
+------------------------+
| all_dim_data[]         |  <- Raw data arrays per header block
+------------------------+
| end_magic (8B)         |  <- 0xe1 'T' 'P' 'B' 0xe1 'E' 0x30 0x31 0xe0
+------------------------+
```

**4) Task Record Domain Header (`TaskRecord.tpbe`)**

追加模式：每个新记录创建时，直接向`.tpbe`文件追加完整的`task_record_t`（128字节）。

**`.tpbe` File Layout：**

```
+------------------------+  <- Byte 0
| magic (8B)             |  <- 0xe1 'T' 'P' 'B' 0xED 'D' 'I' 'R' 0x00
+------------------------+
| version (4B)           |  <- 0x00010000 (version 1.0)
+------------------------+
| reserve (4B)           |  <- Padding
+------------------------+  <- Byte 16 (start of entries)
| task_record_t [0]      |  <- First task record (128 bytes)
| task_record_t [1]      |  <- Second entry (128 bytes)
| ...                    |  <- Appended sequentially
+------------------------+  <- End of file
```

读取时顺序扫描所有128-byte entry，匹配`task_record_id`。

**Note on `utc_bits` encoding:**

The `tpb_dtbits_t` (uint64_t) packs datetime and timezone into a compact 64-bit representation:

| Bit Range | Field | Size | Range |
|-----------|-------|------|-------|
| 0-5       | seconds | 6 bits | 0-59 |
| 6-11      | minutes | 6 bits | 0-59 |
| 12-16     | hours   | 5 bits | 0-23 |
| 17-21     | day     | 5 bits | 1-31 |
| 22-25     | month   | 4 bits | 1-12 |
| 26-33     | year bias | 8 bits | 0-255 (1970-2225) |
| 34-41     | timezone | 8 bits | 15-min increments |
| 42-63     | reserved | 22 bits | unused |

**Year encoding:** Year is stored as a bias from 1970 (e.g., year 2026 is stored as 56).

**Timezone encoding:** Timezone bias is stored in 15-minute increments. The value is signed (two's complement), giving a range of approximately -32 to +31.75 hours.

**Endianness notes:**
The bit layout is defined by bit positions (0-63) and is independent of host byte order. Field access is performed via bit shifting and masking operations that produce identical results on both big-endian and little-endian systems:

```c
/* Example: extracting seconds (bits 0-5) */
uint8_t sec = (uint8_t)((utc_bits >> 0) & 0x3F);  /* Works on any endianness */

/* Example: extracting year bias (bits 26-33) */
uint8_t year_bias = (uint8_t)((utc_bits >> 26) & 0xFF);
uint16_t year = 1970 + year_bias;
```

When `utc_bits` is stored to disk or transmitted over the network, it is stored as a native `uint64_t` value. Readers should treat it as an opaque 64-bit integer and use the same bit extraction operations shown above. Do not cast the memory directly to byte arrays or assume specific byte ordering.

**`btime_ns` vs datetime:**
The `btime_ns` field stores high-precision boot-time (nanoseconds since system boot), which is independent of calendar datetime. For rough conversion from boot-time seconds to calendar datetime:
- 24 hours per day
- 730 hours per month (365/12 * 24)
- 8760 hours per year (365 * 24)

**5) Static Keys**

| Key              | DataType          | Description                         |
| ---------------- | ----------------- | ----------------------------------- |
| TaskRecordID     | string (20B raw)  | Task record's SHA-1 ID              |
| TBatchID         | string (20B raw)  | Foreign key to task batch           |
| KernelID         | string (20B raw)  | Foreign key to kernel definition    |
| StartTimeUTC     | string (ISO 8601) | Invocation start time               |
| StartMachineTime | uint64_t          | Boot time nanoseconds at invocation |
| Duration         | uint64_t          | Execution duration in nanoseconds   |
| ExitCode         | int32_t           | Kernel exit code                    |
| HandleIndex      | uint32_t          | Handle position within batch        |
| MPIRank          | int32_t           | MPI rank or -1                      |

**6) Link ID Formula**

```
SHA1("task" + <UTC_timestamp> + <machine_start_nanoseconds> + <duration> + <hostname> + <username> + <kernel_name> + <pid> + <order_in_batch>)
```


### 2.5. Score Record

The ScoreRecord domain stores benchmark scores calculated through predefined formulas (e.g., from `stream.yaml`). One record per score or sub-score calculation.

| Key | Definition | DataType |
|-----|------------|----------|
| ScoreRecordID | Score record's unique SHA-1 ID | string |
| TBatchID | Parent task batch ID (foreign key) | string |
| ScoreName | Name of the calculated score | string |
| ScoreValue | Numeric value of the score | double |
| BenchFile | Path to benchmark definition YAML file | string |
| ScoreTimeUTC | Timestamp when score was computed | string |

**Link ID Formula:** `SHA1("score" + <UTC_timestamp> + <machine_start_nanoseconds> + <hostname> + <username> + <score_name> + <calc_order>)`

---

### 2.6. OS Record

The OS domain stores static information and runtime configuration/status of the operating system. One record per tbatch (or less frequently if `auto_collect=false`).

| Key | Definition | DataType |
|-----|------------|----------|
| OSID | OS record's unique SHA-1 ID | string |
| OSType | Operating system family (Linux/Windows/macOS/BSD) | string |
| Distribution | OS distribution name (e.g., Ubuntu, CentOS) | string |
| Version | OS version string | string |
| KernelVersion | Kernel version (for Unix-like systems) | string |

**Link ID Formula:** `SHA1("os" + <UTC_timestamp> + <hostname> + <kernel_version>)`

---

### 2.7. CPU Record

The CPUStatic domain stores static information about the CPU(s), including model name, SKU ID, vendor, and runtime configuration. These values do not change during tbatch execution. One record per system (reused across tbatches).

| Key | Definition | DataType |
|-----|------------|----------|
| CPUStaticID | CPUStatic record's unique SHA-1 ID | string |
| VendorID | CPU vendor identifier (e.g., GenuineIntel, AuthenticAMD) | string |
| ModelName | Human-readable model name | string |
| Family | CPU family number | int32_t |
| Model | CPU model number | int32_t |
| Stepping | CPU stepping revision | int32_t |
| NumPhysicalCores | Number of physical cores | int32_t |
| NumLogicalCores | Number of logical processors (threads) | int32_t |
| BaseFrequency | Base clock frequency in MHz | uint32_t |
| MaxTurboFrequency | Maximum turbo frequency in MHz | uint32_t |
| L1CacheSize | L1 cache size per core in KB | uint32_t |
| L2CacheSize | L2 cache size per core in KB | uint32_t |
| L3CacheSize | L3 (shared) cache size in MB | uint32_t |
| HBMSize | On-chip HBM size in MB | uint32_t |
| ISAFlags | Supported instruction set flags (e.g., AVX, AVX2, SSE4.2) | string |

**Link ID Formula:** `SHA1("cpu_static" + <UTC_timestamp> + <hostname> + <vendor_id>)`

---

### 2.8. Memory Record

The MemoryStatic domain stores static information about system memory configuration. These values do not change during tbatch execution. One record per system (reused across tbatches).

| Key | Definition | DataType |
|-----|------------|----------|
| MemoryStaticID | MemoryStatic record's unique SHA-1 ID | string |
| TotalSize | Total installed memory in MB | uint64_t |
| NChannel | Number of memory channels (e.g., 2 for dual-channel) | int32_t |
| ChannelType | Memory type (DDR3/DDR4/DDR5/LRDIMM) | string |
| MainFrequency | Operating frequency in MHz | uint32_t |
| NModule | Number of DIMM modules installed | int32_t |
| ModuleSize | JSON array of individual module sizes | string |
| ECCFlags | ECC technique flags, "None" or "N/A" for non-ECC memory. | string |

**Link ID Formula:** `SHA1("memory_static" + <UTC_timestamp> + <hostname> + <total_capacity_mb>)`

---

### 2.9. Accelerator Record

The AcceleratorStatic domain stores static information about accelerators (GPU, FPGA, etc.). One record per accelerator device.

| Key | Definition | DataType |
|-----|------------|----------|
| AcceleratorStaticID | AcceleratorStatic record's unique SHA-1 ID | string |
| DeviceType | Type of accelerator (GPU/FPGA/ASIC) | string |
| Vendor | Manufacturer name (e.g., NVIDIA, AMD, Intel) | string |
| DeviceName | Model/product name | string |
| DriverVersion | Driver version string | string |
| NDevice | Number of such devices in system | int32_t |
| MemorySize | Device memory size in MB | uint64_t |
| NCore | Number of compute cores/units | int32_t |
| BaseFrequency | Base clock frequency in MHz | uint32_t |

**Link ID Formula:** `SHA1("accelerator_static" + <UTC_timestamp> + <hostname> + <vendor> + <device_name>)`

---

### 2.10. Network Record

The NetworkStatic domain stores aggregated information about network adapters and interfaces. These values do not change during tbatch execution. One record per system (reused across tbatches).

| Key | Definition | DataType |
|-----|------------|----------|
| NetworkStaticID | NetworkStatic record's unique SHA-1 ID | string |
| NInterface | Total number of network interfaces | int32_t |
| IFTrios | JSON array of interface details (name, MAC, speed) | string |

**Link ID Formula:** `SHA1("network_static" + <UTC_timestamp> + <hostname> + <primary_mac_address>)`

---

### 2.11. Motherboard Record

The MotherBoardStatic domain stores static information about the motherboard. These values do not change during tbatch execution. One record per system (reused across tbatches).

| Key | Definition | DataType |
|-----|------------|----------|
| MotherBoardStaticID | MotherBoardStatic record's unique SHA-1 ID | string |
| Manufacturer | Motherboard manufacturer name | string |
| Model | Motherboard model number | string |
| BIOSVersion | BIOS/UEFI firmware version | string |
| BIOSDateUTC | BIOS release date in ISO 8601 format | string |

**Link ID Formula:** `SHA1("motherboard_static" + <UTC_timestamp> + <hostname> + <manufacturer> + <model>)`

---

### 2.12. Chassis Record

The ChassisStatic domain stores static information about the system chassis/enclosure. These values do not change during tbatch execution. One record per system (reused across tbatches).

| Key | Definition | DataType |
|-----|------------|----------|
| ChassisStaticID | ChassisStatic record's unique SHA-1 ID | string |
| Type | Chassis type (Tower/Rack/Blade/Laptop/Desktop/Cluster) | string |
| Manufacturer | Chassis manufacturer name | string |
| Model | Chassis model description | string |
| CoolType | Cooling system type | string |

**Link ID Formula:** `SHA1("chassis_static" + <UTC_timestamp> + <hostname> + <manufacturer> + <model>)`

## 3. Record Backend: `rawdb`

This section explains the design of integrated `rawdb` backend for building TPBench database and supporting CRUD operations to the database. Each domain has its own subdirectory under `${TPB_WORKSPACE}/rawdb/`.


## 4. Record Frontend

### 4.1. List records

List the latest 20 kernel or score records, one record per line. 
```bash
### Following commands are equal.
$ tpbcli record list
$ tpbcli record ls
$ tpbcli r list
$ tpbcli r ls
### --- Output ---
TPBench v1.0
2024-01-01T08:11:30 [I] TPB_HOME: /usr/local/tpbench
2024-01-01T08:11:30 [I] TPB_WORKSPACE: /usr/local/tpbench
2024-01-01T08:11:30 [I] Reading records from /usr/local/tpbench/.tpb_data ... Done
2024-01-01T08:11:30 [I] List of latest 20 records
===
BatchTime,RecordTime,Duration,NumArg,Num
===
2024-01-01T08:11:30 [I] TPBench exit.

```
