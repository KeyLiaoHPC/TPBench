# The Design of the TPBench Record Architecture

TPBench record each kernel's test and help user to use and manage these records. A record is a set of data produced by a kernel invocation, which in corelib is directly derived from the execution of a run-time handle. Moreover, due to the nature that many people start to realize that the actual performance is highly sensitive to the program context and underlying environment, TPBench tags a record with ScoreRecordID, KernelRecordID, and TBatchID, for further analysis in conjunction with other kernels and underlying environment.

## 1. Core Concepts

**Task Batch**  
A task batch (tbatch) is the execution context initiated by a single TPBench front-end invocation (e.g., `tpbcli run` or `tpbcli benchmark`). A tbatch may execute one or more kernel invocations, each with its own input arguments. All kernel invocations within a tbatch share a common TaskBatchID, which tags all records produced during that execution. For rational test results, a tbatch requires a hardware and operating system environment close to the actual scenario. Specifically, if soneone use a script to invoke multiple tpbcli, or if the underlying hardware and/or software configurations change between invocations, each should be treated as a separate tbatch.

**TBatchID**  
A unique identifier for a run batch, generated as:
```
SHA1("rbatch" + <UTC_timestamp> + <hostname> + <username> + <front_end_pid>)
```
Example: `SHA1("rbatch20000308T130801Znode01testuser13249")`

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
| **Kernel Record** | `SHA1("kernel" + <timestamp> + <hostname> + <username> + <kernel_name> + <pid> + <order_in_batch>)` | One record per kernel invocation (handle execution) |
| **Score Record** | `SHA1("score" + <timestamp> + <hostname> + <username> + <score_name> + <calc_order>)` | One record per calculated score or sub-score |

**Note:** The "handle" concept is an internal implementation detail. End users interact with "kernel invocations" without needing to understand handle management.
