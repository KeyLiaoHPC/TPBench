# The Data Record Component Design

## 1. Introduction

### 1.1. Rationale and Architecture

TPBench records data that influences kernel test outputs and captures results produced by kernel invocations. Due to the nature that actual performance is highly sensitive to program context and underlying environment, TPBench tags each record with a dedicated ID for further analysis in conjunction with other kernels and environmental factors.

Target metrics are measurements resulting from a complex parameter space of computing hardware states, software stack configurations, and workload input arguments/data. Theoretically, one could take snapshots of the entire system along with target metrics to build a comprehensive database covering performance, power, portability, accuracy, and resilience characteristics. However, the trade-off between overhead and observation granularity in computing systems is inevitable.

Eventually, data recording frequency, technique varies between data aspects. TPBench uses multiple **data domains** to record different aspects of the computing system, such as system states, kernel definitions, input arguments, target metrics, etc. Different records have predefined recording trigger and are linked via SHA-1 IDs. This approach avoids recording complete snapshots for each test while maintaining traceability.

Each domain has fixed-name record and dynamic record. Fixed-named records are attributes used to characterize the basic information of each piece of record in a domain, and dynamic record is named by configurable record headers with real-time record data:
- **Attribute**: Static attributes characterzing an instance in a domain, recorded in designated byte positions with predefined fixed names, definitions, format and parsers.
- **Header**: Headers are used to define the name and purpose of a single- or multi-dimension record data, including the fixed headers and customize headers. A header is the description section providing information of the record data it holds.
- **Record Data**: Data recorded by TPBench.

In the **rafdb** backend, the **attributes**, **headers** and **record data** are stored in the TPBench entry files and TPBench record files:
- **TPBench Entry File** (`<DomainName>.tpbe`): Recording attributes of each single record in the workspace, starting with a 8-Byte TPBench magic signature as a notation to the file type and the domain. All attribute values in the data is stored positional with fixed data size. Each new record appends a set of attributes as an entry to the record file.
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
SHA1("tbatch" + <utc_bits> + <btime> + <hostname> + <username> + <front_end_pid>)
```
Example: `SHA1("tbatch20250308T130801Z3600000000000node01testuser13249")`

**Kernel**: A kernel is a program or code module that users wish to evaluate. Kernels reside in `${TPB_HOME}/lib` or `${TPB_WORKSPACE}/lib/` and are named `tpbk_<kernel_name>.<so|x>`. The editable source catalog **`kernel_list.cmake.in`** (under **`src/kernels/`**) lists available CPU kernel names, tags, and source paths; **`tpbcli kernel list`** may show **`N/A`** for catalog entries not yet built into **`lib/`**. Compiled KernelID records in rafdb are separate from registry tags.

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

**Argument**: An argument is the concrete value (number, string, or data reference) supplied to a parameter during workload execution. Arguments are provided via CLI options (`--kargs`, `--kenvs`, `--wrapper`) or configuration files.

---

**Target Metric**: A target metric is a quantity that users wish to measure, observe, or derive from a workload execution. The most common metric is runtime. In TPBench, metrics may also include:
- Performance metrics (throughput, latency, FLOPS)
- Correctness, accuracy, or other arithmetic precision indicators
- Resource usage (power, memory bandwidth)
- Any user-defined observable from the workload

---

**Domain / Record**: A Record stores data that influences kernel test outputs and results produced by kernel invocations. Each Record includes a unique SHA-1 ID and related ID to link and be linked. Each record belongs to a domain, which classifies different kinds of records into different aspects, and allow different data to be accquired and recorded in different scenarios.



## 2. The Record Data Architecture

### 2.1. Logical Organization of Database

TPBench's records are stored and managed under a workspace, except for special cross-workspace tools. TPBench uses two environment variables to navigate the filesystem: `TPB_HOME` is the TPBench installation path, and `TPB_WORKSPACE` is the root folder of a TPBench project.

**1) Workspace Resolution:**
1. If `${TPB_WORKSPACE}` environment variable is explicitly set, use that directory as the workspace.
2. Otherwise, search `$HOME/.tpbench/`.

For more information related to the build system and filesystem structure, refer to [design_build.md](./design_build.md).

---

**2) File Structure**

TPBench implement a integrated `rafdb` backend for data management and operations. More backend (e.g. SQLite, HDF5) will be added in future.

In `rafdb`, each record domain has two kinds of file, the TPBench header (.tpbe) and the TPBench record (.tpbr). `.tpbe` is the incremental header that chases all record of the domain in current workspace, and `.tpbr` stores parts of attributes, headers, and detail record data.

File tree:
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

**3) Record Link Topology**

![TPBench record link topology](./arts/record_link_topology.svg)

The `.tpbr` files are named with ID of the corresponding entry `.tpbe`, the `corelib` handles cross-domain and cross-file access through IDs.

---

**4) General Header**

Various record data types are the main body of TPBench records. Each type is defined by a header. The tpb_meta_header structure stores the header's information. Each `.tpbr` file contains one metadata block which encloses headers (`header[i]`) that describe the dimensions, data type, and layout of the record data.

```c
typedef struct tpb_meta_header {
    uint32_t block_size;                          /**< The header size in Bytes */
    uint32_t ndim;                                /**< Number of dimensions, in [0, TPBM_DATA_NDIM_MAX] */
    uint64_t data_size;                           /**< The header's record data size in Bytes */
    uint32_t type_bits;                           /**< Data type control bits: TPB_PARM_SOURCE_MASK | */
                                                  /**< TPB_PARM_CHECK_MASK | TPB_PARM_TYPE_MASK */
    uint32_t _reserve;                            /**< Reserved padding for alignment */
    uint64_t uattr_bits;                          /**< Metric unit encoding, aligned to TPB_UNIT_T */
    char name[TPBM_NAME_STR_MAX_LEN];            /**< Name, length in [0, TPBM_NAME_STR_MAX_LEN] */
    char note[TPBM_NOTE_STR_MAX_LEN];            /**< Notes and descriptions */
    uint64_t dimsizes[TPBM_DATA_NDIM_MAX];       /**< Dimension sizes: dimsizes[0]=innermost (d0) */
    char dimnames[TPBM_DATA_NDIM_MAX][64];       /**< Dimension names, parallel to dimsizes */
} tpb_meta_header_t;                              /**< 2840 Bytes (fixed size on disk) */
```

The `dimsizes` and `dimnames` arrays are fixed-width inline arrays with `TPBM_DATA_NDIM_MAX` (7) slots. Only the first `ndim` slots are meaningful; unused slots are zero-filled. Each slot in `dimnames` is a 64-byte fixed-width string. Each slot in `dimsizes` stores the number of elements in that dimension.

The `type_bits` field encodes the data type using 32-bit masks: `TPB_PARM_SOURCE_MASK` (bits 24-31), `TPB_PARM_CHECK_MASK` (bits 16-23), and `TPB_PARM_TYPE_MASK` (bits 0-15). The `uattr_bits` field encodes the metric unit using the `TPB_UNIT_T` encoding from `tpb-unitdefs.h`.

For multi-dimension record, dimension 0 is the innermost dimension. For a 2D array X[2][4] with dimsizes[0]=4 and dimsizes[1]=2, the record data file's layout is:
```
Address: 0x00    0x01    0x02    0x03    0x04    0x05    0x06    0x07
Data:    X[0][0] X[0][1] X[0][2] X[0][3] X[1][0] X[1][1] X[1][2] X[1][3]
``` 

---

**5) Timestamp**
A uint64_t utc_bits is used to encode date time and ready for ISO-8601 representation. And a uint64_t `btime` is used to store high-precision boot-time (nanoseconds since system boot), which is independent of calendar datetime. For rough conversion from boot-time seconds to calendar datetime:
- 24 hours per day
- 730 hours per month (365/12 * 24)
- 8760 hours per year (365 * 24)

The `typedef uint64_t tpb_dtbits_t` packs datetime and timezone into a compact 64-bit representation:

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

---

**6) Record Types and IDs**

Each ID is a 20-byte SHA1 has string stored in a 20-byte unsigned char variable.

| Record Type | ID | Description |
|-------------|-----------|-------------|
| **TBatch Record** | `SHA1("tbatch" + <utc_bits> + <btime> + <hostname> + <username> + <front_end_pid>)` | One record per tbatch invocation |
| **Kernel Record** | `<so_sha1>` (SHA-1 of the `libtpbk_<name>.so` file) | Check or generate once a kernel is built. |
| **Task Record** | `SHA1("task" + <utc_bits> + <btime> + <hostname> + <username> + <tbatch_id> + <kernel_id> + <order_in_batch> + <pid> + <tid>)` | One record per kernel invocation |
| **Score Record** | `SHA1("score" + <utc_bits> + <btime> + <hostname> + <username> + <score_name> + <calc_order>)` | One record per calculated score or sub-score |

**Magic Signature**

Magic signature is a special 8-byte unsigned char strings tagging the boundary of `.tpbe` and `.tpbr`, including the start/end of the whole entries block, the start of meta block and the start/end of the record data block. The format of magic signature is:
|Byte Position|00|01|02|03|04|05|06|07|
|---|---|---|---|---|---|---|---|---|
|Hex|E1|54|50|42|\<X\>|\<Y\>|31|E0|
|ASCII text|.|T|P|B|.|S/D/E|1|.|

**X**: Domain mark, constructing with two 4-bit mark: `0x<HI><LO>`. E.g. The start of tbatch `.tpbe` file is marked by `0xE1 0x54 0x50 0x42 0xE0 0x53 0x31 0xE0`.
- HI: The high 4 bits. File type mark. `.tpbe: 0xE`; `.tpbr: 0xD`.
- LO: The low 4 bits. Domain type mark. TBatch: `0`; kernel: `1`, task: `2`.

**Y**: Position mark. File start: `0x53 ('S')`. File end: `0x45 ('E')`. Block splitter: `0x44 ('D')`.

### 2.2. Task Batch Record

A task batch (tbatch) is the execution context that invokes one or more kernels. Each `tpbcli` call or dedicated kernel execution (e.g., directly invoking `tpbk_<stream>.x`) creates a new tbatch. TBatchID serves as the Link ID for each tbatch record.

#### 2.2.1. Attributes

```c
typedef struct tbatch_attr {
    unsigned char tbatch_id[20];        /**< TBatchID - Primary Link ID (20-byte SHA-1) */
    unsigned char derive_to[20];           /**< Derivation target: 0=none, else other TBatchID */
    unsigned char inherit_from[20];         /**< Lineage: source TBatchID if copied/forked, else zero */
    tpb_dtbits_t utc_bits;              /**< Batch start datetime (64-bit compact encoding) */
    uint64_t btime;                     /**< Boot time at batch start (nanoseconds since boot) */
    uint64_t duration;                  /**< Total batch duration in nanoseconds */
    char hostname[64];                  /**< Execution host name, ASCII */
    char username[64];                  /**< Username who initiated batch, ASCII */
    uint32_t front_pid;                 /**< Front-end process ID */
    uint32_t nkernel;                   /**< Number of non-repeat kernels executed in this batch */
    uint32_t ntask;                     /**< Task entry points (derive_to==0) in this batch */
    uint32_t nscore;                    /**< Number of score records in this batch */
    uint32_t batch_type;                /**< 0=run, 1=benchmark */
    uint32_t nheader;                   /**< # of headers. */
    tpb_meta_header_t *headers;
} tbatch_attr_t;
```

The implementation program takes responsibility to add and manage fixed headers in corresponding domain files. In task batch records, there are three fixed header members:
- header[0]: KernelRecordIDs
    - 20-byte unsigned char
    - A non-repeated list of KernelRecordIDs of executed kernels in the tbatch, ordering from 0 to nkernel-1 from the oldest start-up time.
    - ndim = 1, length = nkernel
- header[1]: TaskRecordIDs
    - 20-byte unsigned char
    - A non-repeated list of TaskRecordIDs for **task entry points** in the tbatch (rows with `derive_to` all-zero: standalone tasks and capsules, not per-rank rows that derive to a capsule).
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

**2) `derive_to` and `inherit_from`**

- `derive_to`: non-zero means this record aliases another TBatchID (deduplication target).
- `inherit_from`: non-zero records the source TBatchID when this record was copied or derived for lineage; independent of `derive_to`.

#### 2.2.2. Entry Structure (.tpbe)

File structure:
```
+----------------------0-+  
| meta_magic (8B)        |  <- 0xe1 'T' 'P' 'B' 0xe0 'S' 0x31 0xe0
+----------------------8-+ 
| entry[0:N] (264B)      |
+-----------------8+264N-+
| end_magic (8B)         |  <- 0xe1 'T' 'P' 'B' 0xe0 'E' 0x31 0xe0
+----------------16+264N-+
```

**Migration:** Older 128-byte `.tpbe` / prior `.tpbr` fixed layouts are incompatible; remove or re-initialize `rafdb/` in the workspace after upgrading. If you still have a `rawdb/` tree from an older release, rename it to `rafdb/` (or merge into `rafdb/`) so paths match the current layout.

Entry member:
|name|size|
|---|---|
|tbatch_id | 20 |
|inherit_from | 20 |
|start_utc_bits | 8 |
|duration | 8 |
|hostname | 64 |
|nkernel | 4 |
|ntask | 4 |
|nscore | 4 |
|batch_type | 4 |
|reserve | 128 (`TPB_RAF_RESERVE_SIZE`) |
|**Total**|264|



Magic signature:

| Magic      | Text |Hex Value                      | Purpose              |
| ---------- | ---  |------------------------------ | -------------------- |
| entry_begin_magic | . T P B . S 1 . |`E1 54 50 42 E0 53 31 E0`   | Entry file begin |
| entry_end_magic | . T P B . E 1 . |`E1 54 50 42 E0 45 31 E0`   | Entry file end |

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
| 128-Byte reserve     |  <- `TPB_RAF_RESERVE_SIZE`, opaque
+------------------388-+
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
| meta_magic        | . T P B . S 0 . | `E1 54 50 42 D0 53 31 E0` | Meta section begin   |
| record_magic      | . T P B . D 0 . | `E1 54 50 42 D0 44 31 E0` | Record data section  |
| end_magic         | . T P B . E 0 . | `E1 54 50 42 D0 45 31 E0` | End of tpbr          |

**Note**: ID arrays (TaskRecordIDs and ScoreRecordIDs) are now stored as header blocks within the meta section, not as raw bytes after the fixed header.


### 2.3. Kernel Record

The Kernel domain stores metadata about a kernel being evaluated, including description, version, parameter definitions, and metric definitions. One record per unique kernel (shared across invocations).

#### 2.3.1. Attributes

```c
typedef struct kernel_attr {
    unsigned char kernel_id[20];        /**< KernelID - Primary Link ID (20-byte SHA-1), equals kernel .so SHA-1 */
    unsigned char derive_to[20];           /**< Derivation target: 0=none, else other KernelID, ASCII */
    unsigned char inherit_from[20];         /**< Lineage: source KernelID if copied/forked, else zero */

    char kernel_name[256];              /**< Kernel name (without tpbk_ prefix), utf-8 */
    char version[64];                   /**< Version string, utf-8 */
    char description[2048];             /**< Human-readable description, utf-8 */

    uint32_t nparm;                     /**< Number of registered parameters */
    uint32_t nmetric;                   /**< Number of registered output metrics */
    uint32_t kctrl;                     /**< Kernel control bits (PLI=2) */
    uint32_t nheader;                   /**< # of headers (= nparm + nmetric + 3 metadata) */
    uint32_t active;                    /**< 1 if loadable variant, 0 if inactive/historical */
    tpb_dtbits_t utc_bits;              /**< Kernel build/registration datetime (UTC) */
} kernel_attr_t;
```

Fixed header members:
- header[0..nparm-1]: Parameter definitions
    - Each `tpb_meta_header_t` describes one parameter: name, dtype, ndim, dims.
    - Record data: default value for each parameter.
    - ndim >= 1, length determined by parameter definition.
- header[nparm..nparm+nmetric-1]: Metric definitions
    - Each `tpb_meta_header_t` describes one output metric: name, dtype, ndim, dims.
    - Record data: unit encoding for each metric (static definitions use `data_size = 0`).
    - ndim >= 1, length determined by metric definition.
- header[nparm+nmetric .. nparm+nmetric+2]: Compile-history metadata (string payloads)
    - **`variation`**: kernel name, KernelID, `active`, `.so` path
    - **`compilation`**: compiler id/version/path, C flags, kernel-specific flags, CMake build type
    - **`dependency`**: libraries linked above the kernel (e.g. `libtpbench.so`)
    - Payload format: `key=value\n` lines (`format=tpbench.kernel_meta.v1`, `section=<name>`)
    - Header type: `TPB_STRING_T`, `ndim = 1`, `dimsizes[0] = payload_len + 1`

KernelID:
```
<so_sha1>
```

KernelID is the SHA-1 digest of the PLI kernel module `libtpbk_<name>.so`.

Notes:
- `kctrl` stores the kernel integration type from `tpb-public.h`. Always set to `TPB_KTYPE_PLI`.
- `derive_to` is all-zero for canonical records; otherwise it points to the canonical `kernel_id`.
- `inherit_from` is all-zero unless this record was derived from another kernel record (provenance).
- `active` is stored in both `.tpbe` entries and `.tpbr` attributes. Only one KernelID per kernel name is typically active; older variants are marked inactive when a new `.so` replaces `lib/libtpbk_<name>.so`.
- `utc_bits` records when the kernel `.so` was registered (build/install time), not task execution time. CLI dump displays it as **`Build datetime (UTC)`**.
- When a KernelID already exists, registration and metadata updates are skipped unless `TPB_K_OVERRIDE` is set to a truthy value.

#### 2.3.2. Entry Structure (.tpbe)

File structure:
```
+----------------------0-+
| entry_begin_magic (8B) |  <- 0xe1 'T' 'P' 'B' 0xe1 'S' 0x31 0xe0
+----------------------8-+
| entry[0:N] (264B)      |
+-----------------8+264N-+
| entry_end_magic (8B)   |  <- 0xe1 'T' 'P' 'B' 0xe1 'E' 0x31 0xe0
+----------------16+264N-+
```

Entry member (slim subset of `kernel_attr_t`):

| name | size |
|---|---|
| kernel_id | 20 |
| inherit_from | 20 |
| kernel_name | 64 |
| kctrl | 4 |
| nparm | 4 |
| nmetric | 4 |
| active | 4 |
| utc_bits | 8 |
| reserve | 136 (`TPB_RAF_RESERVE_SIZE + 8`) |
| **Total** | **264** |

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
| derive_to               |  <- 20B
+-------------------64-+
| inherit_from             |  <- 20B
+-------------------84-+
| kernel_name          |  <- 256B
+------------------340-+
| version              |  <- 64B
+------------------404-+
| description          |  <- 2048B
+-----------------2452-+
| nparm                |  <- 4B
+-----------------2456-+
| nmetric              |  <- 4B
+-----------------2460-+
| kctrl                |  <- 4B
+-----------------2464-+
| nheader              |  <- 4B
+-----------------2468-+
| active               |  <- 4B
+-----------------2472-+
| utc_bits             |  <- 8B
+-----------------2480-+
| 180-Byte reserve     |  <- `TPB_RAF_KERNEL_ATTR_RESERVE`, opaque
+-----------------2660-+
| fixed_headers[i]     |  <- (nparm + nmetric + 3) x tpb_meta_header_t + user headers
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
- header[nparm+nmetric .. nparm+nmetric+2]: `variation`, `compilation`, `dependency` metadata

Rules:
- Header ordering is deterministic: all parameters first, then all metrics, then the three metadata headers.

Magic signature:

| Magic             | Text            | Hex Value                 | Purpose              |
| ----------------- | --------------- | ------------------------- | -------------------- |
| meta_magic        | . T P B . S 1 . | `E1 54 50 42 D1 53 31 E0` | Meta section begin   |
| record_magic      | . T P B . D 1 . | `E1 54 50 42 D1 44 31 E0` | Record data section  |
| end_magic         | . T P B . E 1 . | `E1 54 50 42 D1 45 31 E0` | End of tpbr          |

### 2.4. Task Record

The TaskRecord domain stores the input arguments and output metrics from a single kernel invocation. One record per handle execution.

#### 2.4.1. Attributes

```c
typedef struct task_attr {
    unsigned char task_record_id[20];   /**< TaskRecordID - Primary Link ID (20-byte SHA-1) */
    unsigned char derive_to[20];           /**< Derivation target: 0=none, else other TaskRecordID */
    unsigned char inherit_from[20];         /**< Lineage: source TaskRecordID if copied/forked, else zero */
    unsigned char tbatch_id[20];        /**< Foreign key: links to task batch that produced this record */
    unsigned char kernel_id[20];        /**< Foreign key: links to kernel definition record */

    tpb_dtbits_t utc_bits;              /**< Invocation datetime (64-bit compact encoding) */
    uint64_t btime;                     /**< Boot time at invocation (nanoseconds since boot) */
    uint64_t duration;                  /**< Kernel execution duration in nanoseconds */

    uint32_t exit_code;                 /**< Kernel exit code (0=success) */
    uint32_t handle_index;              /**< Handle index within batch (0-based) */
    uint32_t pid;                       /**< Writer process ID */
    uint32_t tid;                       /**< Writer thread ID */
    uint32_t ninput;                    /**< # of input argument headers */
    uint32_t noutput;                   /**< # of output metric headers */
    uint32_t nheader;                   /**< # of headers (= ninput + noutput) */
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
SHA1("task" + <utc_bits> + <btime> + <hostname> + <username> + <tbatch_id> + <kernel_id> + <order_in_batch> + <pid> + <tid>)
```

Notes:
- `task_record_id` uniqueness is scoped by full hash ingredients and should be globally unique in a workspace.
- `derive_to` points to the canonical task record when deduplication is enabled.
- For an MPI kernel that finalizes a task capsule, each rank's normal task record has `derive_to` set to the **TaskCapsuleRecordID** that groups all ranks for that invocation.
- `inherit_from` is all-zero unless this record was derived from another task record (provenance).

**Task entry point (`ntask`, CLI):** Physically, `task.tpbe` contains every appended task row. For batch metadata and `tpbcli database ls`, `ntask` counts only rows with `derive_to` all-zero (one logical invocation: a normal single-rank task or a task capsule after MPI finalize). Per-rank rows that point `derive_to` at a capsule ID are not counted. `tpbcli database dump -dt -e` lists only entry-point rows and prints `(N task entry points shown / M total / K rows)`.

#### 2.4.2. Entry Structure (.tpbe)

File structure:
```
+----------------------0-+
| entry_begin_magic (8B) |  <- 0xe1 'T' 'P' 'B' 0xe2 'S' 0x31 0xe0
+----------------------8-+
| entry[0:N] (232B)      |
+-----------------8+232N-+
| entry_end_magic (8B)   |  <- 0xe1 'T' 'P' 'B' 0xe2 'E' 0x31 0xe0
+----------------16+232N-+
```

Entry member (slim subset of `task_attr_t`):

| name | size |
|---|---|
| task_record_id | 20 |
| inherit_from | 20 |
| tbatch_id | 20 |
| kernel_id | 20 |
| utc_bits | 8 |
| duration | 8 |
| exit_code | 4 |
| handle_index | 4 |
| derive_to | 20 |
| reserve | 108 |
| **Total** | **232** |

Magic signature:

| Magic             | Text            | Hex Value                 | Purpose              |
| ----------------- | --------------- | ------------------------- | -------------------- |
| entry_begin_magic | . T P B . S 1 . | `E1 54 50 42 E2 53 31 E0` | Entry file begin     |
| entry_end_magic   | . T P B . E 1 . | `E1 54 50 42 E2 45 31 E0` | Entry file end       |

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
| 128-Byte reserve     |  <- `TPB_RAF_RESERVE_SIZE`, opaque
+------------------308-+
| headers[i]           |  <- (ninput + noutput) x tpb_meta_header_t + user headers
+-------------metasize-+
| record_magic         |  <- 8B
+-----------metasize+8-+
| record_data          |  <- stored per dim, size, type in headers
+--metasize+datasize-8-+
| end_magic            |  <- 8B
+----metasize+datasize-+
```

Byte layout summary:
- Fixed task attribute bytes: `152 -> 156` after adding `pid` and `tid` and
  removing `mpi_rank`
- Fixed meta bytes before headers: `24 + 156 + 128 = 308`
- Previous implementation header start: `304`
- New implementation header start: `308`
- The old document omitted the scalar `reserve` field, so its diagram moves
  from `300` to `308`
- Older task `.tpbr` files are not layout-compatible with this definition

Header member:
- header[0..ninput-1]: Input argument data headers
- header[ninput..ninput+noutput-1]: Output metric data headers

Rules:
- Header ordering is deterministic: all input headers first, then output headers, then user-defined.
- `record_data` stores header payloads in the same order as their headers.

Magic signature:

| Magic             | Text            | Hex Value                 | Purpose              |
| ----------------- | --------------- | ------------------------- | -------------------- |
| meta_magic        | . T P B . S 1 . | `E1 54 50 42 D2 53 31 E0` | Meta section begin   |
| record_magic      | . T P B . D 1 . | `E1 54 50 42 D2 44 31 E0` | Record data section  |
| end_magic         | . T P B . E 1 . | `E1 54 50 42 D2 45 31 E0` | End of tpbr          |

### 2.4.4. Task Capsule Record

A **task capsule record** is a special task-domain `.tpbr` that does not store kernel arguments or performance outputs. It groups every per-rank or per-thread **TaskRecordID** produced by a single logical kernel invocation into one file for analysis.

**Purpose:** Multi-thread or multi-process runs each produce a normal task record with a distinct TaskRecordID. A capsule provides one link record whose payload is only the list of those IDs, without merging argument/metric headers.

**TaskCapsuleRecordID:**

```
SHA1("taskcapsule" + <utc_bits> + <btime> + <hostname> + <username>
     + <tbatch_id> + <kernel_id> + <order_in_batch> + <pid> + <tid>)
```

The capsule uses the **same** `utc_bits`, `btime`, `hostname`, `username`, `tbatch_id`, `kernel_id`, `handle_index` (order), `pid`, and `tid` as the **first** writer (e.g. MPI rank 0 or the primary thread). Only the leading hash literal differs from a normal task ID (`"task"` vs `"taskcapsule"`).

**Attributes (`task_attr_t`):**

- `inherit_from`: all bytes `0xFF` (multi-source sentinel; same convention as merged task records).
- `derive_to`: all zero.
- `ninput` = 0, `noutput` = 0, `nheader` = 1.
- `exit_code`, `duration`: typically 0 for the capsule itself.

**Fixed header (single):**

- **Name:** `TPBLINK::TaskID`
- **Semantics:** 1-D array of 20-byte task record IDs, stored contiguously in the record data section. The inner dimension length grows as member IDs are appended to the capsule `.tpbr` under advisory locking (see lifecycle below).

**Entry (`.tpbe`):** Same `task.tpbe` row layout as a normal task entry; the capsule appears as another task row distinguished by its TaskCapsuleRecordID.

**Per-rank task linkage:** After a successful MPI capsule finalize, each participating rank's normal task `.tpbr` and matching `task.tpbe` row have `derive_to` set to the capsule's TaskCapsuleRecordID (`tpb_k_task_set_derive_to`).

**Lifecycle (kernel responsibility):**

**MPI (recommended):** After `MPI_Init`, each rank initializes corelib (rank 0 first with console output, sub-ranks silently), runs the kernel, then coordinates capsule finalize locally: each rank writes its task via `tpb_record_write_task` or `tpb_k_write_task`; barrier; rank 0 `tpb_k_create_capsule_task`; `MPI_Bcast` of capsule ID and status; each rank patches its own task `derive_to` to the capsule; `MPI_Gather` of all TaskRecordIDs to rank 0; rank 0 alone calls `tpb_k_append_capsule_task` for ranks 1..n-1 in MPI rank order (sub ranks do not touch the capsule file). See `tpbk_stream_mpi.c` for the reference implementation.

**Ad-hoc / non-MPI multi-unit:**

1. Each unit writes its normal task record via `tpb_k_write_task` and obtains its TaskRecordID (optional output pointer).
2. The designated leader (thread 0 or rank 0) calls `tpb_k_create_capsule_task(first_task_id, capsule_id_out)`, which creates the capsule `.tpbr` with the first ID in the data section and appends the capsule entry.
3. Other units synchronize on the capsule ID (e.g. POSIX shared memory `tpb_k_sync_capsule_task`, or `MPI_Bcast` in an MPI kernel).
4. Each non-leader calls `tpb_k_append_capsule_task(capsule_id, own_task_id)`. Appends use an advisory file lock on the capsule `.tpbr`: read the header dimension and `datasize`, strip the trailing end magic, append 20 bytes, rewrite the end magic, then update `datasize` and the header's `dimsizes[0]` / `data_size`.

**Magic bytes:** Same task-domain record magics as section 2.4.3 (`.tpbr` task domain).

#### 2.4.4.1 Accessing MPI Results

After an `stream_mpi` run:

1. Find the TBatchID from `tpbcli db list`:

   ```bash
   tpbcli db list
   ```

2. Dump the TBatch record to get the `TaskRecordIDs` list (includes capsule IDs):

   ```bash
   tpbcli db dump -dT -i <TBatchID>
   ```

   Look for the `TPBLINK::TaskID` header data which lists the capsule ID(s).

3. Identify the capsule ID (it will be in the TaskRecordIDs array, usually the first entry).

4. Dump the capsule to see all rank TaskRecordIDs:

   ```bash
   tpbcli db dump -dt -i <CapsuleID>
   ```

   The output includes a `TPBLINK::TaskID` header with a 1-D array of 20-byte SHA1 IDs for all ranks.

5. Pick a rank's TaskRecordID (e.g., rank 0) and dump it:

   ```bash
   tpbcli db dump -dt -i <Rank0ID>
   ```

   This will show the actual `ntest`, `stream_array_size` arguments and the output metrics (`copy_bw_walltime`, `scale_bw_walltime`, `add_bw_walltime`, `triad_bw_walltime`) from that rank.

**Important:** All MPI ranks compute the same metrics; the capsule does not store metrics itself, only the group membership. For aggregate bandwidth, the kernel output (printed to terminal and log file) already shows the total across all ranks.

### 2.5. Score Record

**TODO: This section is not implemented in current version, need further design.**

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

**TODO: This section is not implemented in current version, need further design.**

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


**TODO: This section is not implemented in current version, need further design.**

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


**TODO: This section is not implemented in current version, need further design.**

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

**TODO: This section is not implemented in current version, need further design.**

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

**TODO: This section is not implemented in current version, need further design.**

The NetworkStatic domain stores aggregated information about network adapters and interfaces. These values do not change during tbatch execution. One record per system (reused across tbatches).

| Key | Definition | DataType |
|-----|------------|----------|
| NetworkStaticID | NetworkStatic record's unique SHA-1 ID | string |
| NInterface | Total number of network interfaces | int32_t |
| IFTrios | JSON array of interface details (name, MAC, speed) | string |

**Link ID Formula:** `SHA1("network_static" + <UTC_timestamp> + <hostname> + <primary_mac_address>)`

---

### 2.11. Motherboard Record

**TODO: This section is not implemented in current version, need further design.**

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

**TODO: This section is not implemented in current version, need further design.**

The ChassisStatic domain stores static information about the system chassis/enclosure. These values do not change during tbatch execution. One record per system (reused across tbatches).

| Key | Definition | DataType |
|-----|------------|----------|
| ChassisStaticID | ChassisStatic record's unique SHA-1 ID | string |
| Type | Chassis type (Tower/Rack/Blade/Laptop/Desktop/Cluster) | string |
| Manufacturer | Chassis manufacturer name | string |
| Model | Chassis model description | string |
| CoolType | Cooling system type | string |

**Link ID Formula:** `SHA1("chassis_static" + <UTC_timestamp> + <hostname> + <manufacturer> + <model>)`

## 3. Record Backend: `rafdb`

### 3.1. Module Structure

The `rafdb` backend is organized in three layers under `src/corelib/rafdb/`:

```
src/corelib/rafdb/
├── rafdb-l1-types.h              # L1 constants, magic, path limits
├── rafdb-l1-internal.h           # L1 cross-file internal API
├── rafdb-l1-domain-reg.c/.h      # Domain registry (cross-domain catalog)
├── rafdb-l1-magic.c              # Magic signature build, validate, detect
├── rafdb-l1-sha1.c / .h          # SHA1 + tpb_raf_hash_file
├── rafdb-l1-id-util.c            # tpb_raf_id_to_hex / hex_to_id
├── rafdb-l1-workspace.c          # Workspace resolve + init (registry-driven)
├── rafdb-l1-entry.c              # Generic .tpbe append/list + flock
├── rafdb-l1-record-io.c          # Header serialization, free_headers
├── rafdb-l1-record-path.c        # Entry/record path building
├── rafdb-l1-record-locate.c      # tpb_raf_find_record (cross-domain)
├── rafdb-l1-scan.c               # resolve_record_file, scan_by_id_prefix
├── rafdb-l2-tbatch.c             # TBatch domain entry/record/ID
├── rafdb-l2-kernel.c             # Kernel domain entry/record/ID
├── rafdb-l2-task.c               # Task domain entry/record/ID, create_capsule
├── rafdb-l2-kernel-meta-build.c  # build_registered_attr
├── rafdb-l3-tbatch-taglink.c     # TPBLINK:: append to tbatch .tpbr
├── rafdb-l3-task-taglink.c       # TPBLINK:: capsule append
├── rafdb-l3-kernel-kvmeta.c      # Kernel metadata kv helpers
├── rafdb-l3-kernel-patch.c       # Kernel active patch / deactivate
├── tpb-raf-types.h               # Alias → rafdb-l1-types.h
├── tpb-sha1.h                    # Alias → rafdb-l1-sha1.h
└── tpb-raf-kernel-meta.h         # Kernel L2/L3 constants
```

**Layer rules:** L1 is domain-agnostic (including cross-domain scan/locate). L2 is strictly per-domain CRUD. L3 is strictly per-domain decoration; filenames must include the domain name (`rafdb-l3-<domain>-*.c`).

See also [`docs/design/rafdb_architecture.md`](rafdb_architecture.md) for the dependency diagram.

All public types and function declarations are in `src/include/tpb-public.h`, merged into the generated flat `include/tpbench.h` at build/install time.

### 3.2. Workspace Resolution

`tpb_raf_resolve_workspace()` is called at the start of every `tpbcli` subcommand. Resolution order:

1. If `$TPB_WORKSPACE` is set and non-empty, use that directory.
2. If `$HOME/.tpbench/etc/config.json` exists and contains a `"name"` field, use `$HOME/.tpbench/`.
3. Otherwise, create a default workspace at `$HOME/.tpbench/` with `etc/config.json` (`{"name": "default"}`) and `rafdb/{task_batch,kernel,task}/` directories.

### 3.3. API Summary

Workspace:
- `tpb_raf_resolve_workspace(out_path, pathlen)` -- resolve workspace path
- `tpb_raf_init_workspace(workspace_path)` -- create directory structure and config

Magic:
- `tpb_raf_build_magic(ftype, domain, pos, out)` -- construct 8-byte magic
- `tpb_raf_validate_magic(magic, ftype, domain, pos)` -- validate magic bytes

Entry (.tpbe):
- `tpb_raf_entry_append_{tbatch,kernel,task}(workspace, entry)` -- append entry
- `tpb_raf_entry_list_{tbatch,kernel,task}(workspace, entries, count)` -- list all entries

Record (.tpbr):
- `tpb_raf_record_write_{tbatch,kernel,task}(workspace, attr, data, datasize)` -- write record
- `tpb_raf_record_read_{tbatch,kernel,task}(workspace, id, attr, data, datasize)` -- read record
- `tpb_raf_record_patch_tbatch_counters(workspace, tbatch_id, duration, nkernel, ntask)` -- patch tbatch counters
- `tpb_raf_record_append_tbatch(workspace, tbatch_id, task_id)` -- append TaskID to tbatch TPBLINK header (`rafdb-l3-tbatch-taglink.c`)
- `tpb_raf_record_create_task_capsule(workspace, attr, first_task_id)` -- new task capsule
- `tpb_raf_record_append_task_capsule(workspace, capsule_id, task_id)` -- append member ID (`rafdb-l3-task-taglink.c`)
- `tpb_raf_free_headers(headers, nheader)` -- free allocated header arrays

Cross-domain (L1):
- `tpb_raf_find_record(workspace, id, domain_out)` -- locate .tpbr by ID
- `tpb_raf_resolve_record_file(workspace, inpath, resolved, cap)` -- resolve user path
- `tpb_raf_scan_records_by_id_prefix(...)` / `tpb_raf_free_id_matches(...)` -- prefix scan
- `tpb_raf_hash_file(filepath, sha1_out)` -- file SHA1

Kernel metadata (L2/L3):
- `tpb_raf_kernel_build_registered_attr(...)` / `tpb_raf_kernel_free_built_attr(...)`
- `tpb_raf_kernel_find_header`, `tpb_raf_kernel_meta_kv_get/set`, `tpb_raf_kernel_update_meta_key`
- `tpb_raf_entry_patch_kernel_active`, `tpb_raf_record_patch_kernel_active`, `tpb_raf_kernel_deactivate_same_name`

ID Generation:
- `tpb_raf_gen_tbatch_id(utc_bits, btime, hostname, username, pid, id_out)` -- TBatchID
- `tpb_raf_gen_kernel_id(tpbx_sha1, id_out)` -- KernelID (copy kernel .so SHA-1)
- `tpb_raf_gen_task_id(utc_bits, btime, hostname, username, tbatch_id, kernel_id, order, pid, tid, id_out)` -- TaskRecordID
- `tpb_raf_gen_taskcapsule_id(...)` -- same inputs as task ID, leading literal `"taskcapsule"` -- TaskCapsuleRecordID
- `tpb_raf_id_to_hex(id, hex)` -- convert 20-byte ID to 40-char hex string

### 3.4. Header Serialization

On-disk format per `tpb_meta_header_t` (fixed 3096 bytes upper bound; breaking vs older 2840):

```
block_size       (4B)
ndim             (4B)
data_size        (8B)
type_bits        (4B)   -- TPB_PARM_SOURCE_MASK | TPB_PARM_CHECK_MASK | TPB_PARM_TYPE_MASK
_reserve         (4B)   -- reserved padding
uattr_bits       (8B)   -- metric unit encoding (TPB_UNIT_T)
name             (256B) -- local name (no embedded tags)
tag              (256B) -- normalized tags (may be empty); NEW (incompatible with pre-tag .tpbr)
note             (2048B)
dimsizes[0..6]   (56B)  -- 7 x uint64_t, only first ndim slots are meaningful
dimnames[0..6]   (448B) -- 7 x 64-byte strings, parallel to dimsizes
```

### 3.5. Memory Safety

- Entry structs (`tbatch_entry_t` / `kernel_entry_t` 264 bytes, `task_entry_t`
  232 bytes) are stack-allocated or caller-provided.
- `tpb_meta_header_t` arrays are heap-allocated during record read; freed via `tpb_raf_free_headers()`.
- All pointer arguments are NULL-checked; functions return `TPBE_NULLPTR_ARG` on failure.
- File I/O uses explicit size checks; no buffer overruns.

## 4. Record Frontend

The record frontend is implemented in `src/tpbcli_record.c` and dispatched via the `record` (or `r`) subcommand in `src/tpbcli.c`.

### 4.1. List records

`tpbcli record list` (aliases: `tpbcli record ls`, `tpbcli r list`, `tpbcli r ls`) lists the latest 20 tbatch entries from the workspace, sorted by start time (newest first).

```bash
$ tpbcli record list
### --- Output ---
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

Column details:

| Column | Source | Format |
|--------|--------|--------|
| Start Time (UTC) | `start_utc_bits` decoded | `YYYY-MM-DDThh:mm:ssZ` (ISO 8601) |
| TBatch ID | `tbatch_id` | 40-character hex string |
| Type | `batch_type` | `run` (0) or `benchmark` (1) |
| NTask | `ntask` | integer |
| NKernel | `nkernel` | integer |
| NScore | `nscore` | integer (always 0 for now) |
| Duration (s) | `duration` nanoseconds | seconds with 3 decimal digits |

## 5. Parallel Recording

### 5.1. Overview

When a kernel executes across multiple threads or processes, each execution unit writes its own task record independently.

**Preferred grouping (task capsule):** A **task capsule record** (section 2.4.4) lists all related TaskRecordIDs in one `.tpbr` without duplicating arguments or metrics. **MPI kernels** coordinate collectives in the kernel source and use capsule APIs: rank 0 creates the capsule and appends other ranks' IDs after `MPI_Gather`; each rank's task record `derive_to` (`.tpbr` and `task.tpbe` row) points at the capsule ID. **Non-MPI multi-unit:** the leader creates the capsule after each unit has written its task record; other units append their IDs under file locking. Shared memory (`tpb_k_sync_capsule_task`) or `MPI_Bcast` can distribute the capsule ID.

A hybrid kernel (multi-process x multi-thread) may use capsules per process then a top-level capsule that lists the per-process capsule IDs.

## 6. Naming Format of Headers

### 6.1. Role

`tpb_meta_header_t.name` is the stable local label used for lookup, dumps, scoring, and cross-kernel comparison. `tpb_meta_header_t.tag` holds optional classification tags (separate field).

### 6.2. Canonical Layout

- **name:** non-empty local name; no `:` characters; ≤255 characters; unique across a kernel's arguments and outputs.
- **tag:** optional comma-separated tokens stored without spaces after normalize (uppercase, deduped, sorted ascending). Display printers may insert `", "` between tokens.

```
name = Triad bandwidth
tag  = BANDWIDTH,FOM,TPBOUT   (storage)
Tags: BANDWIDTH, FOM, TPBOUT  (CLI display)
```

Kernel registration: `tpb_k_add_arg(name, user_tag, …)` / `tpb_k_add_output(name, user_tag, …)`. System appends role tags **TPBARG** (argument) or **TPBOUT** (output), then normalizes. User tag text ≤191 characters (room for system tags).

### 6.3. Lexical Rules

Prefer letters, digits, underscores, and spaces in `name`. **Comma** separates tag tokens only. Avoid `:` in name or tag.

### 6.4. Reserved Tag Vocabulary

| Tag | Meaning |
| --- | --- |
| **TPBARG** | System role: kernel argument (CLI/recorded input). |
| **TPBOUT** | System role: kernel output metric. |
| **TPBLINK** | Internal linkage (name=`TaskID` / `KernelID` / `DeriveTo`). |
| **FOM** | Figure of merit; primary outcome metrics. |
| **INPARM** | Input snapshot recorded as data (output column). |
| **EVENT** | In-run samples (counters, markers). |
| **PERF** | Performance-related metrics, figures, indicators, and counters. |
| **POWER** | Energy / power. |
| **PRECISION** | Precision / error metrics. |
| **TIME** | Time, tick, duration, and intervals. |
| **BANDWIDTH** | Volume per time (e.g. MB/s). |
| **RATE** | Volume per counter tick (e.g. MB/s, physical_day/ns, token/s). |
| **COUNT** | Counts. |
| **PERCENT** | Percent / normalized fraction as %. |

Reuse these before adding new tags; extend the table in this document when needed.

### 6.5. Lookup and Search Behavior

Runtime APIs match **name** exactly (`strcmp`). Tag is not part of the lookup key. Tools that previously matched `Tag::Name` strings should match local `name` (and optionally filter on `tag`).

### 6.6. Example (STREAM-style Names)

Examples: name=`Copy` tag=`EVENT,TIME,TPBOUT`; name=`Triad bandwidth` tag=`BANDWIDTH,FOM,TPBOUT`. `tpbcli run` prints `Name:` / `Tags:` lines; `tpbcli database dump` prints both fields as stored.

### 6.7. CLI dump layout (`.tpbr`, `-i`)

`tpbcli db dump -i` prints a human-readable view aligned with on-disk section markers:

| File region | Dump output |
|-------------|-------------|
| START magic | `0x E1 54 …` line before **Section: Metadata** |
| Fixed attributes + `header[i]` | **Attributes** / **Headers** subsections; `name = value` rows; `type_bits` / `uattr_bits` decoded in parentheses |
| SPLIT magic | Magic line before **Section: Record Data** |
| Payload blobs | Per-header axis title (non-zero `dimsizes` only); innermost dimension on one line: `[i][j][]: v0, v1, …` |
| END magic | Magic line + **END OF FILE** |

`tpbcli db dump -e` (`.tpbe` index) remains CSV-style `key, value` rows.
