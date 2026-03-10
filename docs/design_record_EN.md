# The Data Record Component Design

TPBench records data that influences kernel test outputs and captures results produced by kernel invocations. Due to the nature that actual performance is highly sensitive to program context and underlying environment, TPBench tags each record with a dedicated ID for further analysis in conjunction with other kernels and environmental factors.

## 1. Core Concepts

**Task Batch (TBatch)**: A task batch (tbatch) is the execution context initiated by a single TPBench front-end invocation (e.g., `tpbcli run`). A tbatch may execute one or more kernel invocations, each with its own input arguments. All kernel invocations within a tbatch share a common TBatchID, which tags all records produced during that execution. For valid test results, a tbatch requires stable run-time environment matching the actual scenario. Specifically:
- If someone uses a script to invoke multiple `tpbcli` commands sequentially, each invocation should be treated as a separate tbatch.
- If underlying hardware or software configurations change between invocations, each configuration state constitutes a separate tbatch. (Please don't trouble yourself.)

**TBatchID**: A unique identifier for a task batch, generated as:
```
SHA1("tbatch" + <UTC_timestamp> + <machine_start_nanoseconds> + <hostname> + <username> + <front_end_pid>)
```
Where `<machine_start_nanoseconds>` is the number of nanoseconds since system boot (uint64_t).
Example: `SHA1("tbatch20250308T130801Z3600000000000node01testuser13249")`

---

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

**Domain / Record**: A Record stores data that influences kernel test outputs and results produced by kernel invocations. Each Record includes:
- **Link ID**: A unique identifier for this record; other records reference it to establish relationships.
- **Fixed keys and values**: Several fixed-name keys are defined by TPBench for each domain to enable tracing of similar parameters across platforms.
- **Dynamic keys and values**: Many hardware and software designs have specific parameters; TPBench allows adding custom keys to records.

Each record belongs to a domain, which classifies different kinds of records into different aspects, and allow different data to be accquired and recorded in different scenarios.

---

**Record Types and IDs**

| Record Type | ID Format | Description |
|-------------|-----------|-------------|
| **Task Record** | `SHA1("kernel" + <UTC_timestamp> + <machine_start_nanoseconds> + <hostname> + <username> + <kernel_name> + <pid> + <order_in_batch>)` | One record per kernel invocation |
| **Score Record** | `SHA1("score" + <UTC_timestamp> + <machine_start_nanoseconds> + <hostname> + <username> + <score_name> + <calc_order>)` | One record per calculated score or sub-score |

## 2. The Design of Data Record Logic in TPBench Corelib

### 2.1. Logical Organization of Database

TPBench's records are stored and managed under a workspace, except for special cross-workspace tools. TPBench uses two environment variables to navigate the filesystem: `TPB_HOME` is the TPBench installation path, and `TPB_WORKSPACE` is the root folder of a TPBench project.

**Workspace Resolution:**
1. If `${TPB_WORKSPACE}` environment variable is explicitly set, use that directory as the workspace.
2. Otherwise, search `$TPB_HOME/.tpb_workspace/` for workspace configuration files.
3. If not found there, search `$HOME/.tpb_workspace/`.

For more information related to the build system and filesystem structure, refer to [design_build.md](./design_build.md).

---

**Design Rationale:**

Target metrics are measurements resulting from a complex combination of computing hardware states, software stack configurations, and workload input arguments/data. Theoretically, one could take snapshots of the entire system along with target metrics to build a comprehensive database covering performance, power, portability, accuracy, and resilience characteristics. However, the trade-off between overhead and observation granularity in computing systems is inevitable.

Therefore, TPBench uses multiple **data domains** to record system states, kernel definitions, input arguments, target metrics, and related factors. Different domains are linked via SHA-1 IDs called **Link IDs** (similar to primary keys in relational databases). This approach avoids recording complete snapshots for each test while maintaining traceability.

Each domain has:
- **Static keys**: Fixed names and definitions established by TPBench
- **Dynamic keys**: Customizable fields to accommodate varying hardware and software designs

Each domain records a specific aspect of system status that characterizes the states, inputs, and outputs of task batches and kernel invocations. However, recording all domains at every kernel invocation leads to significant overhead. Therefore, settings in `${TPB_WORKSPACE}/etc/config.json` can control recording behavior:
- `auto_collect`: Boolean flag to enable/disable automatic collection for a domain
- `action`: Trigger condition for recording (e.g., `"kernel_invoke"`, `"user_invoke"`)

---

**Domain Definitions:**

**1) Domain: TaskBatch**

A task batch (tbatch) is the execution context that invokes one or more kernels. Each `tpbcli` call or dedicated kernel execution (e.g., directly invoking `tpbk_<stream>.x`) creates a new tbatch. TBatchID serves as the Link ID for each tbatch record.

| Key | Definition | DataType |
|-----|------------|----------|
| TBatchID | Task batch SHA-1 Link ID | string |
| TaskRecordIDn | List of task record IDs within this batch | list<string> |
| ScoreRecordIDs | List of score record IDs within this batch | list<string> |
| StartTimeUTC | Start time in ISO 8601 format (YYYY-MM-DDThh:mm:ssZ) | string |
| StartMachineTime | Start time as nanoseconds since boot | uint64_t |
| Duration | Total duration of the tbatch in nanoseconds | uint64_t |
| Hostname | Hostname where the tbatch executed | string |
| User | Username who initiated the tbatch | string |

**Link ID Formula:** `SHA1("tbatch" + <UTC_timestamp> + <machine_start_nanoseconds> + <hostname> + <username> + <front_end_pid>)`

---

**2) Domain: Kernel**

The Kernel domain stores metadata about a kernel being evaluated, including description, version, parameter definitions, and metric definitions. One record per unique kernel (shared across invocations).

| Key | Definition | DataType |
|-----|------------|----------|
| KernelID | Kernel domain's unique identifier | string |
| KernelName | Name of the kernel (without `tpbk_` prefix) | string |
| Version | Kernel version string | string |
| Description | Human-readable description of the kernel's purpose | string |
| SourceFile | Path to source file used for compilation | string |
| CompileDateUTC | Compilation timestamp in ISO 8601 format | string |
| ParameterDefinitions | JSON array describing input parameters | string |
| MetricDefinitions | JSON array defining measurable outputs | string |

**Link ID Formula:** `SHA1("kernel_def" + <kernel_name> + <version> + <source_file_hash>)`

---

**3) Domain: TaskRecord**

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

**Link ID Formula:** `SHA1("kernel" + <UTC_timestamp> + <machine_start_nanoseconds> + <hostname> + <username> + <kernel_name> + <pid> + <order_in_batch>)`

---

**4) Domain: ScoreRecord**

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

**5) Domain: OS**

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

**6) Domain: CPUStatic**

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

**7) Domain: MemoryStatic**

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

**8) Domain: AcceleratorStatic**

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

**9) Domain: NetworkStatic**

The NetworkStatic domain stores aggregated information about network adapters and interfaces. These values do not change during tbatch execution. One record per system (reused across tbatches).

| Key | Definition | DataType |
|-----|------------|----------|
| NetworkStaticID | NetworkStatic record's unique SHA-1 ID | string |
| NInterface | Total number of network interfaces | int32_t |
| IFTrios | JSON array of interface details (name, MAC, speed) | string |

**Link ID Formula:** `SHA1("network_static" + <UTC_timestamp> + <hostname> + <primary_mac_address>)`

---

**10) Domain: MotherBoardStatic**

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

**11) Domain: ChassisStatic**

The ChassisStatic domain stores static information about the system chassis/enclosure. These values do not change during tbatch execution. One record per system (reused across tbatches).

| Key | Definition | DataType |
|-----|------------|----------|
| ChassisStaticID | ChassisStatic record's unique SHA-1 ID | string |
| Type | Chassis type (Tower/Rack/Blade/Laptop/Desktop/Cluster) | string |
| Manufacturer | Chassis manufacturer name | string |
| Model | Chassis model description | string |
| CoolType | Cooling system type | string |

**Link ID Formula:** `SHA1("chassis_static" + <UTC_timestamp> + <hostname> + <manufacturer> + <model>)`

### 2.2. Record Backend: `rawdb`

This section explains the design of integrated `rawdb` backend for building TPBench database and supporting CRUD operations to the database. Each domain has its own subdirectory under `${TPB_WORKSPACE}/rawdb/`.

``` C

// 112-Byte task-record header
typedef struct task_record {
    unsigned char kernel_record_id[20]; /**< TaskRecordID */
    unsigned char dup_to[20];           /**< Init to 0, point to the other TaskRecordID if a duplicate-modify operation is executed. */
    unsigned char tbatch_id[20];        /**< TBatchID links to tbatch record that produces this record */
    unsigned char kernel_id[20];        /**< KernelID links to the kernel record that produces this record */
    tpb_dtbits_t utc_bits;              /**< Dense 64-bit datetime (tpb_dtbits_t): year-month-day-hour-min-sec-tz */
    uint64_t btime_ns;                  /**< Boot time in nanoseconds (seconds*1e9 + nsec since system boot) */
    uint64_t duration;                  /**< kernel invocation duration in nanoseconds */
    uint32_t exit_code;                 /**< Exit code */
} task_record_t;

```

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

## 3. Record Frontend

### 3.1. List records

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
