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

## 2. The Record Data Architecture

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

**3) Record Link Topology**

![TPBench record link topology](./arts/record_link_topology.svg)

**4) General Header**

Various record data types are the main body of TPBench records. Each type is defined by a header. The tpb_meta_header structure describes the general header for dynamic record data. Each `.tpbr` file contains one metadata block which encloses headers (`header[i]`) that describe the dimensions, data type, and layout of the record data.

```c
typedef struct tpb_dim_info{
    unsigned char name[256];    /**< Dimension name, length in [0, 256], can be empty */
    uint64_t length;            /**< Number of elements in this dimension >= 1 */
} tpb_dim_info_t;                   /**< 262 Bytes */

typedef struct tpb_meta_header {
    uint32_t block_size;        /**< The header size in Bytes */
    uint32_t ndim;              /**< Number of dimensions, in [1, 7] */
    uint64_t data_size;         /**< The header's record data size in Bytes */
    uint64_t type_bits;         /**< Data type control bits, including element */
                                /**< size and TPB_*_T type, supports custom types */
    unsigned char name[256];    /**< Name, length in [0, 256] */
    unsigned char note[2048];   /**< Notes and descriptions */
    tpb_dim_info_t *dim_info;   /**< Pointer to dimensions info */
} tpb_meta_header_t;            /**< 1312 Bytes */
```

### 2.2. Task Batch Record

A task batch (tbatch) is the execution context that invokes one or more kernels. Each `tpbcli` call or dedicated kernel execution (e.g., directly invoking `tpbk_<stream>.x`) creates a new tbatch. TBatchID serves as the Link ID for each tbatch record.

#### 2.2.1. Attributes

```c
typedef struct tbatch_attr {
    unsigned char tbatch_id[20];        /**< TBatchID - Primary Link ID (20-byte SHA-1) */
    unsigned char dup_to[20];           /**< Duplicate tracking: 0=none, else points to other TBatchID */
    tpb_dtbits_t utc_bits;              /**< Batch start datetime (64-bit compact encoding) */
    uint64_t btime;                     /**< Boot time at batch start (nanoseconds since boot) */
    uint64_t duration;                  /**< Total batch duration in nanoseconds */
    char hostname[64];                  /**< Execution host name */
    char username[64];                  /**< Username who initiated batch */
    uint32_t front_pid;                 /**< Front-end process ID */
    uint32_t nkernel;                   /**< Number of non-repeat kernels executed in this batch */
    uint32_t ntask;                     /**< Number of task records in this batch */
    uint32_t nscore;                    /**< Number of score records in this batch */
    uint32_t nheader;                   /**< # of fixed headers. */
    uint32_t nuheader;                  /**< # of user-defined headers. */
    tpb_meta_header_t fixed_headers[3];
    tpb_meta_header_t *uheaders;
} tbatch_attr_t;
```

Fixed header members:
- header[0]: KernelRecordIDs
    - 20-byte unsigned char
    - A non-repeated list of KernelRecordIDs of executed kernels in the tbatch, ordering from 0 to nkernel-1 from the oldest start-up time.
    - ndim = 1, length = nkernel
- header[1]: TaskRecordIDs
    - 20-byte unsigned char
    - A non-repeated list of TaskRecordIDs of ended tasks in the tbatch.
    - ndim = 1, length = ntask
- header[2]: ScoreRecordIDs
    - 20-byte unsigned char
    - A non-repeated list of calculated scores of benchmarks in the tbatch.
    - ndim = 1, length = nscore

**Note:**

**1) TBatchID calculation**

```
SHA1("tbatch" + <UTC_timestamp> + <machine_start_nanoseconds> + <hostname> + <username> + <front_end_pid>)
```

Example: `SHA1("tbatch20250308T130801Z3600000000000node01testuser13249")`


**2) Conversion between `btime` and datetime:**
The `btime` field stores high-precision boot-time (nanoseconds since system boot), which is independent of calendar datetime. For rough conversion from boot-time seconds to calendar datetime:
- 24 hours per day
- 730 hours per month (365/12 * 24)
- 8760 hours per year (365 * 24)

**3) `utc_bits` encoding:**

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

Encoding methods:

- **Year encoding:** Year is stored as a bias from 1970 (e.g., year 2026 is stored as 56).

- **Timezone encoding:** Timezone bias is stored in 15-minute increments. The value is signed (two's complement), giving a range of approximately -32 to +31.75 hours.

- **Endianness notes:**
The bit layout is defined by bit positions (0-63) and is independent of host byte order. Field access is performed via bit shifting and masking operations that produce identical results on both big-endian and little-endian systems:

```c
/* Example: extracting seconds (bits 0-5) */
uint8_t sec = (uint8_t)((utc_bits >> 0) & 0x3F);  /* Works on any endianness */

/* Example: extracting year bias (bits 26-33) */
uint8_t year_bias = (uint8_t)((utc_bits >> 26) & 0xFF);
uint16_t year = 1970 + year_bias;
```

When `utc_bits` is stored to disk or transmitted over the network, it is stored as a native `uint64_t` value. Readers should treat it as an opaque 64-bit integer and use the same bit extraction operations shown above. Do not cast the memory directly to byte arrays or assume specific byte ordering.

#### 2.2.2. Entry Structure (.tpbe)

File structure:
```
+----------------------0-+  
| meta_magic (8B)        |  <- 0xe1 'T' 'P' 'B' 0xd0 'S' 0x31 0xe0
+----------------------8-+ 
| entry[0:N] (128B)      |
+-----------------8+128N-+
| end_magic (8B)         |  <- 0xe1 'T' 'P' 'B' 0xd0 'E' 0x31 0xe0
+----------------16+128N-+
```

Entry member:
|name|size|
|---|---|
|tbatch_id | 20 |
|start_utc_bits | 8 |
|duration | 8 |
|hostname | 64 |
|nkernel | 4 |
|ntask | 4 |
|reserve | 20 |
|**Total**|128|



Magic signature:

| Magic      | Text |Hex Value                      | Purpose              |
| ---------- | ---  |------------------------------ | -------------------- |
| entry_begin_magic | . T P B . S 1 . |`E1 54 50 42 D0 53 31 E0`   | Entry file begin |
| entry_end_magic | . T P B . E 1 . |`E1 54 50 42 D0 45 31 E0`   | Entry file end |

#### 2.2.3. Record Structure (.tpbr)

A `.tpbr` record file consists of a **meta section** and a **record data section**. The meta section stores the batch attributes (from `tbatch_attr_t`, see 2.2.1) and the 3 fixed headers that describe the record data layout. The record data section stores data according to the dimension, size, and type defined by the fixed headers.

File structure:
```
+--------------------0-+
| meta_magic           |  <- 8B
+--------------------8-+
| metasize             |  <- 8B, size of meta section
+-------------------16-+
| datasize             |  <- 8B, size of record data section
+-------------------24-+
| tbatch_id            |  <- 20B
+-------------------44-+
| dup_to               |  <- 20B
+-------------------64-+
| utc_bits             |  <- 8B
+-------------------72-+
| btime                |  <- 8B
+-------------------80-+
| duration             |  <- 8B
+-------------------88-+
| hostname             |  <- 64B
+------------------152-+
| username             |  <- 64B
+------------------216-+
| front_pid            |  <- 4B
+------------------220-+
| nkernel              |  <- 4B
+------------------224-+
| ntask                |  <- 4B
+------------------228-+
| nscore               |  <- 4B
+------------------232-+
| nheader              |  <- 4B
+------------------236-+
| 64-Byte reserve      |  <- Reserved for future. 
+------------------300-+
| fixed_headers[i]     |  <- 3 x tpb_meta_header_t and customize headers
+-------------metasize-+
| record_magic         |  <- 8B
+-----------metasize+8-+
| record_data          |  <- stored per dim, size, type in headers
+--metasize+datasize-8-+
| end_magic            |  <- 8B
+----metasize+datasize-+
```

Magic signature:

| Magic             | Text            | Hex Value                 | Purpose              |
| ----------------- | --------------- | ------------------------- | -------------------- |
| meta_magic        | . T P B . S 0 . | `E1 54 50 42 E0 53 31 E0` | Meta section begin   |
| record_magic      | . T P B . D 0 . | `E1 54 50 42 E0 44 31 E0` | Record data section  |
| end_magic         | . T P B . E 0 . | `E1 54 50 42 E0 45 31 E0` | End of tpbr          |

**Note**: ID arrays (TaskRecordIDs and ScoreRecordIDs) are now stored as header blocks within the meta section, not as raw bytes after the fixed header.


### 2.3. Kernel Record

The Kernel domain stores metadata about a kernel being evaluated, including description, version, parameter definitions, and metric definitions. One record per unique kernel (shared across invocations).

#### 2.3.1. Attributes

```c
typedef struct kernel_attr {
    unsigned char kernel_id[20];        /**< KernelID - Primary Link ID (20-byte SHA-1) */
    unsigned char dup_to[20];           /**< Duplicate tracking: 0=none, else points to other KernelID */
    unsigned char src_sha1[20];         /**< SHA-1 hash of concatenated source files */
    unsigned char so_sha1[20];          /**< SHA-1 hash of shared library file */
    unsigned char bin_sha1[20];         /**< SHA-1 hash of executable file, all-zero for FLI-only kernels */

    char kernel_name[256];              /**< Kernel name (without tpbk_ prefix) */
    char version[64];                   /**< Version string */
    char description[2048];             /**< Human-readable description */

    uint32_t nparm;                     /**< Number of registered parameters */
    uint32_t nmetric;                   /**< Number of registered output metrics */
    uint32_t kctrl;                     /**< Kernel control bits (FLI=1, PLI=2, ALI=4) */
    uint32_t nheader;                   /**< # of fixed headers (= nparm + nmetric) */
    uint32_t nuheader;                  /**< # of user-defined headers */
    uint32_t reserve;                   /**< Padding for 8-byte alignment */
} kernel_attr_t;
```

Fixed header members:
- header[0..nparm-1]: Parameter definitions
    - Each `tpb_meta_header_t` describes one parameter: name, dtype, ndim, dims.
    - Record data: default value for each parameter.
    - ndim >= 1, length determined by parameter definition.
- header[nparm..nparm+nmetric-1]: Metric definitions
    - Each `tpb_meta_header_t` describes one output metric: name, dtype, ndim, dims.
    - Record data: unit encoding for each metric.
    - ndim >= 1, length determined by metric definition.

KernelID:
```
SHA1("kernel" + <kernel_name> + <so_sha1> + <bin_sha1>)
```

Notes:
- `kctrl` reuses `TPB_KTYPE_FLI/TPB_KTYPE_PLI/TPB_KTYPE_ALI` from `tpb-public.h`.
- `dup_to` is all-zero for canonical records; otherwise it points to the canonical `kernel_id`.

#### 2.3.2. Entry Structure (.tpbe)

File structure:
```
+----------------------0-+
| entry_begin_magic (8B) |  <- 0xe1 'T' 'P' 'B' 0xe1 'S' 0x31 0xe0
+----------------------8-+
| entry[0:N] (128B)      |
+-----------------8+128N-+
| entry_end_magic (8B)   |  <- 0xe1 'T' 'P' 'B' 0xe1 'E' 0x31 0xe0
+----------------16+128N-+
```

Entry member (slim subset of `kernel_attr_t`):

| name | size |
|---|---|
| kernel_id | 20 |
| kernel_name | 64 |
| so_sha1 | 20 |
| kctrl | 4 |
| nparm | 4 |
| nmetric | 4 |
| reserve | 12 |
| **Total** | **128** |

Magic signature:

| Magic             | Text            | Hex Value                 | Purpose              |
| ----------------- | --------------- | ------------------------- | -------------------- |
| entry_begin_magic | . T P B . S 1 . | `E1 54 50 42 E1 53 31 E0` | Entry file begin     |
| entry_end_magic   | . T P B . E 1 . | `E1 54 50 42 E1 45 31 E0` | Entry file end       |

#### 2.3.3. Record Structure (.tpbr)

A `.tpbr` record file consists of a **meta section** and a **record data section**. The meta section stores the kernel attributes (from `kernel_attr_t`, see 2.3.1) and the fixed headers that describe the record data layout. The record data section stores data according to the dimension, size, and type defined by the fixed headers.

File structure:
```
+--------------------0-+
| meta_magic           |  <- 8B
+--------------------8-+
| metasize             |  <- 8B, size of meta section
+-------------------16-+
| datasize             |  <- 8B, size of record data section
+-------------------24-+
| kernel_id            |  <- 20B
+-------------------44-+
| dup_to               |  <- 20B
+-------------------64-+
| src_sha1             |  <- 20B
+-------------------84-+
| so_sha1              |  <- 20B
+------------------104-+
| bin_sha1             |  <- 20B
+------------------124-+
| kernel_name          |  <- 256B
+------------------380-+
| version              |  <- 64B
+------------------444-+
| description          |  <- 2048B
+-----------------2492-+
| nparm                |  <- 4B
+-----------------2496-+
| nmetric              |  <- 4B
+-----------------2500-+
| kctrl                |  <- 4B
+-----------------2504-+
| nheader              |  <- 4B
+-----------------2508-+
| 64-Byte reserve      |  <- Reserved for future.
+-----------------2572-+
| fixed_headers[i]     |  <- (nparm + nmetric) x tpb_meta_header_t + user headers
+-------------metasize-+
| record_magic         |  <- 8B
+-----------metasize+8-+
| record_data          |  <- stored per dim, size, type in headers
+--metasize+datasize-8-+
| end_magic            |  <- 8B
+----metasize+datasize-+
```

Header member:
- header[0..nparm-1]: Parameter definitions
- header[nparm..nparm+nmetric-1]: Metric definitions

Rules:
- `nheader >= nparm + nmetric`. Extra headers beyond `nparm + nmetric` are user-defined.
- Header ordering is deterministic: all parameters first, then all metrics, then user-defined.

Magic signature:

| Magic             | Text            | Hex Value                 | Purpose              |
| ----------------- | --------------- | ------------------------- | -------------------- |
| meta_magic        | . T P B . S 1 . | `E1 54 50 42 E1 53 31 E0` | Meta section begin   |
| record_magic      | . T P B . D 1 . | `E1 54 50 42 E1 44 31 E0` | Record data section  |
| end_magic         | . T P B . E 1 . | `E1 54 50 42 E1 45 31 E0` | End of tpbr          |

### 2.4. Task Record

The TaskRecord domain stores the input arguments and output metrics from a single kernel invocation. One record per handle execution.

#### 2.4.1. Attributes

```c
typedef struct task_attr {
    unsigned char task_record_id[20];   /**< TaskRecordID - Primary Link ID (20-byte SHA-1) */
    unsigned char dup_to[20];           /**< Duplicate tracking: 0=none, else points to other TaskRecordID */
    unsigned char tbatch_id[20];        /**< Foreign key: links to task batch that produced this record */
    unsigned char kernel_id[20];        /**< Foreign key: links to kernel definition record */

    tpb_dtbits_t utc_bits;              /**< Invocation datetime (64-bit compact encoding) */
    uint64_t btime;                     /**< Boot time at invocation (nanoseconds since boot) */
    uint64_t duration;                  /**< Kernel execution duration in nanoseconds */

    uint32_t exit_code;                 /**< Kernel exit code (0=success) */
    uint32_t handle_index;              /**< Handle index within batch (0-based) */
    int32_t  mpi_rank;                  /**< MPI rank (-1 if non-MPI, 0-N if MPI enabled) */
    uint32_t ninput;                    /**< # of input argument headers */
    uint32_t noutput;                   /**< # of output metric headers */
    uint32_t nheader;                   /**< # of fixed headers (= ninput + noutput) */
    uint32_t nuheader;                  /**< # of user-defined headers */
    uint32_t reserve;                   /**< Padding for 8-byte alignment */
} task_attr_t;
```

Fixed header members:
- header[0..ninput-1]: Input arguments
    - Each `tpb_meta_header_t` describes one input argument: name, dtype, ndim, dims.
    - Record data: the actual argument value used in this invocation.
    - ndim >= 1, length determined by argument definition.
- header[ninput..ninput+noutput-1]: Output metrics
    - Each `tpb_meta_header_t` describes one output metric: name, dtype, ndim, dims.
    - Record data: the actual measured result from this invocation.
    - ndim >= 1, length determined by metric definition.

TaskRecordID:
```
SHA1("task" + <tbatch_id> + <kernel_id> + <handle_index> + <utc_bits> + <btime>)
```

Notes:
- `task_record_id` uniqueness is scoped by full hash ingredients and should be globally unique in a workspace.
- `dup_to` points to the canonical task record when deduplication is enabled.

#### 2.4.2. Entry Structure (.tpbe)

File structure:
```
+----------------------0-+
| entry_begin_magic (8B) |  <- 0xe2 'T' 'P' 'B' 0xe2 'S' 0x31 0xe0
+----------------------8-+
| entry[0:N] (128B)      |
+-----------------8+128N-+
| entry_end_magic (8B)   |  <- 0xe2 'T' 'P' 'B' 0xe2 'E' 0x31 0xe0
+----------------16+128N-+
```

Entry member (slim subset of `task_attr_t`):

| name | size |
|---|---|
| task_record_id | 20 |
| tbatch_id | 20 |
| kernel_id | 20 |
| utc_bits | 8 |
| duration | 8 |
| exit_code | 4 |
| handle_index | 4 |
| mpi_rank | 4 |
| reserve | 40 |
| **Total** | **128** |

Magic signature:

| Magic             | Text            | Hex Value                 | Purpose              |
| ----------------- | --------------- | ------------------------- | -------------------- |
| entry_begin_magic | . T P B . S 2 . | `E2 54 50 42 E2 53 31 E0` | Entry file begin     |
| entry_end_magic   | . T P B . E 2 . | `E2 54 50 42 E2 45 31 E0` | Entry file end       |

#### 2.4.3. Record Structure (.tpbr)

A `.tpbr` record file consists of a **meta section** and a **record data section**. The meta section stores the task attributes (from `task_attr_t`, see 2.4.1) and the fixed headers that describe the record data layout. The record data section stores data according to the dimension, size, and type defined by the fixed headers.

File structure:
```
+--------------------0-+
| meta_magic           |  <- 8B
+--------------------8-+
| metasize             |  <- 8B, size of meta section
+-------------------16-+
| datasize             |  <- 8B, size of record data section
+-------------------24-+
| task_record_id       |  <- 20B
+-------------------44-+
| dup_to               |  <- 20B
+-------------------64-+
| tbatch_id            |  <- 20B
+-------------------84-+
| kernel_id            |  <- 20B
+------------------104-+
| utc_bits             |  <- 8B
+------------------112-+
| btime                |  <- 8B
+------------------120-+
| duration             |  <- 8B
+------------------128-+
| exit_code            |  <- 4B
+------------------132-+
| handle_index         |  <- 4B
+------------------136-+
| mpi_rank             |  <- 4B
+------------------140-+
| ninput               |  <- 4B
+------------------144-+
| noutput              |  <- 4B
+------------------148-+
| nheader              |  <- 4B
+------------------152-+
| 64-Byte reserve      |  <- Reserved for future.
+------------------216-+
| headers[i]           |  <- (ninput + noutput) x tpb_meta_header_t + user headers
+-------------metasize-+
| record_magic         |  <- 8B
+-----------metasize+8-+
| record_data          |  <- stored per dim, size, type in headers
+--metasize+datasize-8-+
| end_magic            |  <- 8B
+----metasize+datasize-+
```

Header member:
- header[0..ninput-1]: Input argument data headers
- header[ninput..ninput+noutput-1]: Output metric data headers

Rules:
- `nheader >= ninput + noutput`. Extra headers beyond `ninput + noutput` are user-defined.
- Header ordering is deterministic: all input headers first, then output headers, then user-defined.
- `record_data` stores header payloads in the same order as their headers.

Magic signature:

| Magic             | Text            | Hex Value                 | Purpose              |
| ----------------- | --------------- | ------------------------- | -------------------- |
| meta_magic        | . T P B . S 2 . | `E2 54 50 42 E2 53 31 E0` | Meta section begin   |
| record_magic      | . T P B . D 2 . | `E2 54 50 42 E2 44 31 E0` | Record data section  |
| end_magic         | . T P B . E 2 . | `E2 54 50 42 E2 45 31 E0` | End of tpbr          |






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
