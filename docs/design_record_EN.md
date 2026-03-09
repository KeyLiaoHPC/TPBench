# The Data Record Component Design

TPBench record each kernel's test and help user to use and manage these records. A record is a set of data produced by a kernel invocation, which in corelib is directly derived from the execution of a run-time handle. Moreover, due to the nature that many people start to realize that the actual performance is highly sensitive to the program context and underlying environment, TPBench tags a record with ScoreRecordID, KernelRecordID, and TBatchID, for further analysis in conjunction with other kernels and underlying environment.

## 1. Core Concepts

**Task Batch**  
A task batch (tbatch) is the execution context initiated by a single TPBench front-end invocation (e.g., `tpbcli run` or `tpbcli benchmark`). A tbatch may execute one or more kernel invocations, each with its own input arguments. All kernel invocations within a tbatch share a common TaskBatchID, which tags all records produced during that execution. For rational test results, a tbatch requires a hardware and operating system environment close to the actual scenario. Specifically, if soneone use a script to invoke multiple tpbcli, or if the underlying hardware and/or software configurations change between invocations, each should be treated as a separate tbatch.

**TBatchID**  
A unique identifier for a run batch, generated as:
```
SHA1("tbatch" + <UTC_timestamp> + <machine_start_nanoseconds> + <hostname> + <username> + <front_end_pid>)
```
Example: `SHA1("tbatch20000308T130801Z3600000000000node01testuser13249")`

---

**Kernel**  
A kernel is a program or code module that users wish to evaluate. Kernels reside in `${TPB_HOME}/lib` or `${TPB_WORKSPACE}/lib/` and are named `tpbk_<kernel_name>.<so|x>`.

---

**Workload**  
A workload is the concrete instantiation of a kernel for benchmarking, defined by:
- The kernel name (identifying the code to execute)
- Input parameters (values assigned to the kernel's parameter definitions)
- Data size (problem scale, e.g., array length, memory size)

Workload represents the *configurable* aspects of a benchmark execution. Hardware state, OS configuration, and other environmental factors are *not* part of the workload definition but are recorded separately as execution context.

---

**Parameter**  
A parameter is a named, typed placeholder defined by a kernel, representing a configurable factor that may affect measurement results. Parameters have:
- A name (e.g., `ntest`, `total_memsize`)
- A data type (int, double, string, etc.)
- A default value
- Optional validation constraints (range, list, custom)

---

**Argument**  
An argument is the concrete value (number, string, or data reference) supplied to a parameter during workload execution. Arguments are provided via CLI options (`--kargs`, `--kenvs`, `--kmpiargs`) or configuration files.

---

**Target Metric**  
A target metric is a quantity that users wish to measure, observe, or derive from a workload execution. The most common metric is the runtime. But in TPBench, they may also include:
- Performance metrics (throughput, latency, FLOPS)
- Correctness, accuracy, or other arithmetic precision indicators
- Resource usage (power, memory bandwidth)
- Any user-defined observable from the workload

---

**Record**  
A Record is a persistent measurement artifact capturing the results of a single kernel invocation or score calculation. Each Record includes:
- KernelRecordID
- TBatchID
- ScoreRecordID (only for benchmark score)
- TotalWallTime: Total time of the execution duration in seconds.
- Hostname: The hostname of the system that produces this record.
- Username: The username who produces this record.
- Fields for kernels, parameters, arguments and target metrics.

---

**Record Types and IDs**

| Record Type | ID Format | Description |
|-------------|-----------|-------------|
| **Kernel Record** | `SHA1("kernel" + <UTC_timestamp> + <machine_start_nanoseconds> + <hostname> + <username> + <kernel_name> + <pid> + <order_in_batch>)` | One record per kernel invocation (handle execution) |
| **Score Record** | `SHA1("score" + <UTC_timestamp> + <machine_start_nanoseconds> + <hostname> + <username> + <score_name> + <calc_order>)` | One record per calculated score or sub-score |

**Note:** The "handle" concept is an internal implementation detail. End users interact with "kernel invocations" without needing to understand handle management.

## 2. The Design of Data Record Logic in TPBench Corelib

### 2.1. The logical organization of database**

TPBench's records are stored and managed under a workspace, except for special cross-workspace tools. TPBench uses two environment variables to guide component through the filesystem: `TPB_HOME` is the TPBench installation path, and `TPB_WORKSPACE` is the root folder of a TPBench project. TPBench will search through `TPB_WORKSPACE` By default, TPBench searches `$TPB_HOME/.tpb_workspace` and `$HOME/.tpb_workspace` for the `records/` folder. For more infomation related to the building and file system, refer to [design_build.md](./design_record.md).

Output metric numbers (e.g. performance data, power data, ...) are the measure of the result of a complex of the states of computing hardware, the configuration of software stacks and the input arguments and data of the workload. Theoretically, one can take a snapshot of both the whole system and target metrics and build database around as many as snapshots to get holographic performance/power/protability/accuracy/resiliant database. However, the trade-off between the overhead and observation granularity within computing systems are inevitable. Hence, TPBench uses multiple data domain to record the system states, kernel definitions, input arguments, target metrics and other ralated factors and link different domains with a sha-1 ID, called link ID (similar to the primary key). With this approach, we don't have to record the whole snapshot for each test. Each domain has static keys which have fixed names and definitions, and dynamic keys which can be defined to provide compatibility to varies hardware and software design.

Each domain records a specific aspect of the status that can be used to characterize the states, inputs and outputs of the task batch and kernel invocation. However, recording each domain at each kernel's invocation lead to a significant overhead, hence a setting at ${TPB_WORKSPACE}/etc/config.json can be used to change the recording behaviour.

**1) Domain: Task Batch**

TBatch is the task batch that invokes one or more kernels. Each `tpbcli` call or dedicated kernel execution (e.g. directly invoke tpbk_\<stream\>.x) create a new tbatch. TBatchID is the link id for each tbatch record.

| Key | Definition | 
|-----|------------|
| TBatcnID | Task batch SHA-1 ID | 
| KernelRecordID | A kernel record's SHA-1 ID | 
| ScoreRecordID | A score record's SHA-1 ID | 
| StartTimeUTC| Start-up time in ISO 8601 format <YYYY-MM-DDThh:mm:ssZ> |
| StartMachineTime| Start-up time related to the machine's boot time in nanoseconds |
| DurationTime| Duration of the tbatch in nanoseconds |
| Hostname | The hostname which runs the tbatch |
| User | The user who starts the tbatch |

**2) Domain: Kernel**

Each kernel record stores the description, version, input parameters, target metrics of an program bing evaluated.

**3) Domain: KernelRecord**

Each kernel record stores the input and output of an invocation to the kernel.

**4) Domain: ScoreRecord**

Each score record stores the score calculated through a predefined formula (e.g. stream.yaml).

**5) Domain: OS**

Each OS record stores the static information, run-time configuration and status of the operating system.

**6) Domain: CPUStatic**

Each CPU record stores the static information such as model name, SKU ID, vendor, run-time configuration of the CPU. These static information does not changed during a tbatch execution.

**7) Ddmain: MemoryStatic**
These static information does not changed during a tbatch execution.

**8) Ddmain: AcceleratorStatic**
These static information does not changed during a tbatch execution.

**9) Ddmain: NetworkStatic**
These static information does not changed during a tbatch execution.

**10)* Ddmain: MotherBoardStatic**
These static information does not changed during a tbatch execution.

**11)* Ddmain: ChassisStatic**
These static information does not changed during a tbatch execution.

### 2.2. Record Backend: CSV

Create a csv file for each record in the domain's folder. Each csv has 3 basic columns: Key, Value, Unit. The database is stored under the folder `${TPB_WORKSPACE}/csv`. The tree of csv files:

```
${TPB_WORKSPACE}/csv/ 
```

### 2.3. Record Backend: SQLite

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
