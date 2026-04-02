# libtpbench API Reference

## Overview

TPBench (Test Performance Benchmark) is a flexible benchmarking framework that provides kernel registration, execution, timing, and result reporting capabilities. The library supports multiple data types, unit conversions, statistical analysis, and customizable output formatting.

**Version:** 0.8  
**Language:** C (C11 standard)  
**Library:** `libtpbench.so`

---

## Table of Contents

1. [Error Codes](#error-codes)
2. [Data Types](#data-types)
3. [Unit Definitions](#unit-definitions)
4. [I/O Functions](#io-functions)
5. [Driver & Kernel Management](#driver--kernel-management)
6. [Kernel Registration API](#kernel-registration-api)
7. [Timer Functions](#timer-functions)
8. [Statistics Functions](#statistics-functions)
9. [Unit Conversion Functions](#unit-conversion-functions)
10. [Error Handling](#error-handling)
11. [Record Database API (rafdb)](#record-database-api-rafdb)

---

## Error Codes

All TPBench functions return an `tpb_errno_t` error code. Defined in `tpb-types.h`.

| Code | Name | Description |
|------|------|-------------|
| 0 | `TPBE_SUCCESS` | Operation completed successfully |
| 1 | `TPBE_EXIT_ON_HELP` | Exit after displaying help |
| 2 | `TPBE_CLI_FAIL` | Command-line interface error |
| 3 | `TPBE_FILE_IO_FAIL` | File I/O operation failed |
| 4 | `TPBE_MALLOC_FAIL` | Memory allocation failed |
| 5 | `TPBE_MPI_FAIL` | MPI operation failed |
| 6 | `TPBE_KERN_ARG_FAIL` | Kernel argument error |
| 7 | `TPBE_KERN_VERIFY_FAIL` | Kernel verification failed |
| 8 | `TPBE_LIST_NOT_FOUND` | Item not found in list |
| 9 | `TPBE_LIST_DUP` | Duplicate item in list |
| 10 | `TPBE_NULLPTR_ARG` | Null pointer argument |
| 11 | `TPBE_DTYPE_NOT_SUPPORTED` | Data type not supported |
| 12 | `TPBE_ILLEGAL_CALL` | Illegal function call |
| 13 | `TPBE_KERNEL_NE_FAIL` | Kernel does not exist |
| 14 | `TPBE_KARG_NE_FAIL` | Kernel argument does not exist |

---

## Data Types

Core data structures defined in `tpb-types.h`.

### Timer Structure

```c
typedef struct tpb_timer {
    char name[TPBM_NAME_STR_MAX_LEN];  // Timer name
    TPB_UNIT_T unit;                   // Unit type
    TPB_DTYPE dtype;                   // Data type
    int (*init)(void);                 // Initialization function
    void (*tick)(int64_t *ts);         // Start timing
    void (*tock)(int64_t *ts);         // Stop timing
    void (*get_stamp)(int64_t *ts);    // Get timestamp
} tpb_timer_t;
```

### Runtime Parameters

```c
typedef struct tpb_rt_parm {
    char name[TPBM_NAME_STR_MAX_LEN];        // Parameter name
    char note[TPBM_NOTE_STR_MAX_LEN];        // Description
    tpb_parm_value_t value;                  // Current value
    TPB_DTYPE ctrlbits;                      // Control bits
    int nlims;                               // Number of limits
    tpb_parm_value_t *plims;                 // Limit values
} tpb_rt_parm_t;
```

### Kernel Output

```c
typedef struct tpb_k_output {
    char name[TPBM_NAME_STR_MAX_LEN];        // Output name
    char note[TPBM_NOTE_STR_MAX_LEN];        // Description
    TPB_DTYPE dtype;                         // Data type
    TPB_UNIT_T unit;                         // Unit type
    int n;                                   // Number of elements
    void *p;                                 // Data pointer
} tpb_k_output_t;
```

### Kernel Runtime Handle

```c
typedef struct tpb_k_rthdl {
    tpb_argpack_t argpack;    // Argument package
    tpb_respack_t respack;    // Result package
    tpb_kernel_t kernel;       // Kernel definition
} tpb_k_rthdl_t;
```

### Kernel Definition

```c
typedef struct tpb_kernel {
    tpb_k_static_info_t info;  // Static information
    tpb_k_func_t func;          // Function pointers
} tpb_kernel_t;

typedef struct tpb_k_func {
    int (*k_output_decorator)(void);            // Output decorator
} tpb_k_func_t;
```

### Result Data File

```c
typedef struct tpb_res {
    char header[1024];      // CSV header
    char fname[1024];       // Filename
    char fdir[PATH_MAX];    // Directory path
    char fpath[PATH_MAX];   // Full path
    int64_t **data;         // 2D data array
} tpb_res_t;
```

---

## Unit Definitions

TPBench uses a 64-bit unit encoding system (`TPB_UNIT_T`). Common units defined in `tpb-unitdefs.h`.

### Time Units

| Constant | Unit | Description |
|----------|------|-------------|
| `TPB_UNIT_NS` | Nanosecond | 10⁻⁹ seconds |
| `TPB_UNIT_US` | Microsecond | 10⁻⁶ seconds |
| `TPB_UNIT_MS` | Millisecond | 10⁻³ seconds |
| `TPB_UNIT_SS` | Second | Base time unit |
| `TPB_UNIT_CY` | Cycle | Processor cycle |

### Data Size Units (Binary)

| Constant | Unit | Description |
|----------|------|-------------|
| `TPB_UNIT_BYTE` | Byte | Base unit |
| `TPB_UNIT_KIB` | Kibibyte | 2¹⁰ bytes |
| `TPB_UNIT_MIB` | Mebibyte | 2²⁰ bytes |
| `TPB_UNIT_GIB` | Gibibyte | 2³⁰ bytes |
| `TPB_UNIT_TIB` | Tebibyte | 2⁴⁰ bytes |

### Data Size Units (Decimal)

| Constant | Unit | Description |
|----------|------|-------------|
| `TPB_UNIT_B` | Byte | Base unit |
| `TPB_UNIT_KB` | Kilobyte | 10³ bytes |
| `TPB_UNIT_MB` | Megabyte | 10⁶ bytes |
| `TPB_UNIT_GB` | Gigabyte | 10⁹ bytes |
| `TPB_UNIT_TB` | Terabyte | 10¹² bytes |

### Performance Units

| Constant | Unit | Description |
|----------|------|-------------|
| `TPB_UNIT_FLOPS` | FLOPS | Floating-point operations/sec |
| `TPB_UNIT_KFLOPS` | KFLOPS | 10³ FLOPS |
| `TPB_UNIT_MFLOPS` | MFLOPS | 10⁶ FLOPS |
| `TPB_UNIT_GFLOPS` | GFLOPS | 10⁹ FLOPS |
| `TPB_UNIT_TFLOPS` | TFLOPS | 10¹² FLOPS |
| `TPB_UNIT_BITPS` | bit/s | Bits per second |
| `TPB_UNIT_BYTEPS` | Byte/s | Bytes per second |
| `TPB_UNIT_KIBPS` | KiB/s | Kibibytes per second |
| `TPB_UNIT_MIBPS` | MiB/s | Mebibytes per second |
| `TPB_UNIT_GIBPS` | GiB/s | Gibibytes per second |

---

## I/O Functions

Functions for input/output operations and formatted output. Defined in `tpb-io.h`.

### `tpb_printf`

Format and print messages to stdout with optional timestamp and tag headers.

```c
void tpb_printf(uint64_t mode_bit, char *fmt, ...);
```

**Parameters:**
- `mode_bit`: Mode bit combination of print modes and error tags
- `fmt`: Printf-style format string
- `...`: Variable arguments for format string

**Print Modes:**
- `TPBM_PRTN_M_DIRECT` (0x00): Direct print, ignore headers
- `TPBM_PRTN_M_TS` (0x01): Print timestamp header only
- `TPBM_PRTN_M_TAG` (0x02): Print tag header only
- `TPBM_PRTN_M_TSTAG` (0x03): Print both timestamp and tag headers

**Error Tags:**
- `TPBE_NOTE` (0x00): Informational note
- `TPBE_WARN` (0x10): Warning
- `TPBE_FAIL` (0x20): Failure
- `TPBE_UNKN` (0x30): Unknown

**Output Format:** `YYYY-mm-dd HH:MM:SS [TAG] message`

**Run log file:** When the workspace is initialized (`tpb_corelib_init` / `tpb_k_corelib_init`), corelib opens a timestamped log under `<workspace>/rafdb/log/tpbrunlog_*_<host>.log` via `tpb_log_init()` (declared in `tpb-io.h`, internal to the `tpbench` library). `tpb_printf` mirrors each line to that file when logging is active.

**`TPB_LOG_FILE`:** Before fork/exec of a PLI kernel, the driver calls `tpb_log_cleanup()`, sets this environment variable to the current log path (`TPB_LOG_FILE_ENV` / `"TPB_LOG_FILE"`), and forks. The parent immediately calls `tpb_log_init()` again so it appends to the same file; the child process inherits `TPB_LOG_FILE` and `tpb_log_init()` inside `tpb_k_corelib_init` opens that path in append mode without writing a second session header. This yields one log file for both `tpbcli` and the `.tpbx` kernel. At the start of `tpb_corelib_init()`, `TPB_LOG_FILE` is cleared (`unsetenv`) so a normal CLI session always starts a new timestamped log instead of appending to a stale path.

---

### `tpb_register_kernel`

Register common parameters, scan PLI kernel shared libraries from the install tree, and sync kernel records with the active workspace. Call once before `tpb_query_kernel` or run. Used by `tpbcli` and may be called by embedding applications.

```c
int tpb_register_kernel(void);
```

**Returns:** `0` on success, or a `TPBE_*` error code.

To list kernels programmatically, call `tpb_register_kernel()`, then iterate with `tpb_query_kernel()` (see Kernel Query API).

---

### `tpb_mkdir`

Create a directory recursively (like `mkdir -p`).

```c
int tpb_mkdir(char *dirpath);
```

**Parameters:**
- `dirpath`: Path to the directory to create

**Returns:**
- `0` on success
- `-1` on error

---

### `tpb_print_help_total`

Print overall help message for tpbcli command-line interface.

```c
void tpb_print_help_total(void);
```

---

### `tpb_writecsv`

Write 2D data with header to a CSV file.

```c
int tpb_writecsv(char *path, int64_t **data, int nrow, int ncol, char *header);
```

**Parameters:**
- `path`: File path for output
- `data`: 2D data array [col][row]
- `nrow`: Number of rows
- `ncol`: Number of columns
- `header`: CSV header string

**Returns:**
- `0` on success
- Error code otherwise

---

### `tpb_cliout_args`

Output kernel arguments to the command-line interface.

```c
int tpb_cliout_args(tpb_k_rthdl_t *handle);
```

**Parameters:**
- `handle`: Pointer to kernel runtime handle

**Returns:**
- `TPBE_SUCCESS` on success
- `TPBE_NULLPTR_ARG` if handle is NULL

---

### `tpb_cliout_results`

Output kernel execution results to the command-line interface.

```c
int tpb_cliout_results(tpb_k_rthdl_t *handle);
```

**Parameters:**
- `handle`: Pointer to kernel runtime handle

**Returns:**
- `TPBE_SUCCESS` on success
- `TPBE_NULLPTR_ARG` if handle is NULL

**Note:** Output is based on the output shape attribute. Casting is controlled per-output via `TPB_UATTR_CAST_Y/N` in the unit field.

---

## Driver & Kernel Management

Functions for managing benchmark drivers and kernel execution. Defined in `tpb-driver.h`.

### `tpb_driver_set_timer`

Set timer function for the whole driver.

```c
int tpb_driver_set_timer(tpb_timer_t timer);
```

**Parameters:**
- `timer`: Timer structure with function pointers

**Returns:**
- Error code (0 on success)

---

### `tpb_driver_get_timer`

Get timer function for the driver.

```c
int tpb_driver_get_timer(tpb_timer_t *timer);
```

**Parameters:**
- `timer`: Non-NULL pointer to receive timer via value-copy

**Returns:**
- `0` on success
- Error code otherwise

---

### `tpb_driver_get_nkern`

Get number of registered kernels.

```c
int tpb_driver_get_nkern(void);
```

**Returns:**
- Number of registered kernels

---

### `tpb_driver_get_nhdl`

Get number of handles.

```c
int tpb_driver_get_nhdl(void);
```

**Returns:**
- Number of handles

---

### `tpb_driver_get_kparm_ptr`

Get kernel parameter pointer and type.

```c
int tpb_driver_get_kparm_ptr(const char *kernel_name, const char *parm_name,
                             void **v, TPB_DTYPE *dtype);
```

**Parameters:**
- `kernel_name`: Kernel name (NULL for current handle)
- `parm_name`: Parameter name
- `v`: Output pointer to parameter value (can be NULL)
- `dtype`: Output for parameter dtype (can be NULL)

**Returns:**
- `0` on success
- Error code otherwise

---

### `tpb_driver_set_hdl_karg`

Set kernel argument value for current handle.

```c
int tpb_driver_set_hdl_karg(const char *parm_name, void *v);
```

**Parameters:**
- `parm_name`: Parameter name
- `v`: Pointer to value to set

**Returns:**
- `0` on success
- Error code otherwise

---

### `tpb_driver_add_handle`

Add a handle for a kernel by name.

```c
int tpb_driver_add_handle(const char *kernel_name);
```

**Parameters:**
- `kernel_name`: Kernel name to create handle for

**Returns:**
- `0` on success
- Error code otherwise

**Note:** Creates handle, sets current_rthdl internally, increments nhdl.

---

### `tpb_driver_run_all`

Run all handles starting from index 1.

```c
int tpb_driver_run_all(void);
```

**Returns:**
- `0` on success
- Error code on first failure

---

### `tpb_register_common`

Register common parameters.

```c
int tpb_register_common(void);
```

**Returns:**
- Error code (0 on success)

---

### `tpb_register_kernel`

Initialize kernel registry and register common parameters.

```c
int tpb_register_kernel(void);
```

**Returns:**
- Error code (0 on success)

---

### `tpb_get_nkern`

Get the number of registered kernels.

```c
int tpb_get_nkern(void);
```

**Returns:**
- Number of registered kernels

---

### `tpb_query_kernel`

Query kernel information by ID or name.

Returns the total number of registered kernels. If `kernel_out` is non-NULL,
attempts to look up a kernel and allocate a fully isolated copy.

```c
int tpb_query_kernel(int id, const char *kernel_name, tpb_kernel_t **kernel_out);
```

**Parameters:**
- `id`: Kernel index (>=0). If id >= 0, kernel_name is ignored. If id < 0, kernel_name is used.
- `kernel_name`: Kernel name (used only when id < 0). Can be NULL if id >= 0.
- `kernel_out`: Pointer to a NULL `tpb_kernel_t*` pointer. On success, *kernel_out will point to an allocated kernel copy. On failure or if kernel_out is NULL, *kernel_out is unchanged. Caller must free with `tpb_free_kernel()` and `free()`.

**Returns:**
- Total number of registered kernels. Check *kernel_out to determine if the specific kernel lookup succeeded (non-NULL) or failed (NULL).

**Example:**
```c
// Query kernel by index
tpb_kernel_t *kernel = NULL;
int nkern = tpb_query_kernel(0, NULL, &kernel);
if (kernel != NULL) {
    // Use kernel->info.name, etc.
    tpb_free_kernel(kernel);  // Free nested data
    free(kernel);             // Free struct
}

// Query kernel by name
tpb_kernel_t *kernel = NULL;
tp_kernel(-1, "my_kernel", &kernel);
if (kernel != NULL) {
    // Use kernel...
    tpb_free_kernel(kernel);  // Free nested data
    free(kernel);             // Free struct
}
```

---

### `tpb_free_kernel`

Free memory allocated by `tpb_query_kernel()`.

Frees all nested structures (parms, plims, outs) within the kernel instance.
Note: This does NOT free the kernel struct itself.

```c
void tpb_free_kernel(tpb_kernel_t *kernel);
```

**Parameters:**
- `kernel`: Kernel instance to clean up. Can be NULL (no-op).

**Example:**
```c
// For heap-allocated kernel from tpb_query_kernel:
tp_kernel(-1, "my_kernel", &kernel);
if (kernel != NULL) {
    // Use kernel...
    tpb_free_kernel(kernel);  // Free nested data
    free(kernel);             // Free struct
}

// For inline kernel struct (like hdl->kernel):
tpb_free_kernel(&hdl->kernel);  // Frees nested data only
```

---

### `tpb_run_pli`

Run a PLI kernel via fork/exec. Builds the execution command with environment
variables, MPI arguments, and kernel parameters, then launches the `.tpbx`
executable via shell. The child inherits the parent’s stdout and stderr; the
shared run log is written through `tpb_printf` in both processes using
`TPB_LOG_FILE` as described under `tpb_printf` (run log file).

Declared in `tpb-public.h`.

```c
int tpb_run_pli(tpb_k_rthdl_t *hdl);
```

**Parameters:**
- `hdl`: Runtime handle for the kernel (must be non-NULL)

**Returns:**
- `0` on success
- Error code otherwise

---

### `tpb_driver_clean_handle`

Clean up and free all memory in the kernel runtime handle.

```c
int tpb_driver_clean_handle(tpb_k_rthdl_t *handle);
```

**Parameters:**
- `handle`: Pointer to the kernel runtime handle

**Returns:**
- `0` on successful cleanup
- Error code if any issues occurred

**Note:** Releases all dynamic allocations for output variables and kernel arguments.

---

## Kernel Registration API

Functions for registering custom benchmark kernels. Defined in `tpb-driver.h`.

### `tpb_k_register`

Register a new kernel with a name and description.

```c
int tpb_k_register(const char *name, const char *note);
```

**Parameters:**
- `name`: Kernel name (must be unique)
- `note`: Description/note about the kernel

**Returns:**
- `0` on success
- Error code otherwise

---

### `tpb_k_add_parm`

Add a runtime parameter to the current kernel.

```c
int tpb_k_add_parm(const char *name, const char *note,
                   const char *default_val, TPB_DTYPE dtype, ...);
```

**Parameters:**
- `name`: Parameter name (used for CLI argument matching)
- `note`: Human-readable parameter description
- `default_val`: String representation of default value
- `dtype`: Combined data type: source | check | type
- `...`: Variable arguments based on validation mode

**Data Type Encoding (dtype):**
Format: `0xSSCCTTTT` (32-bit)
- **SS** (bits 24-31): Parameter Source
  - `TPB_PARM_CLI`: Parameter from CLI
  - `TPB_PARM_MACRO`: Parameter from macro
  - `TPB_PARM_FILE`: Parameter from config file
  - `TPB_PARM_ENV`: Parameter from environment variable
- **CC** (bits 16-23): Check/Validation mode
  - `TPB_PARM_NOCHECK`: No validation
  - `TPB_PARM_RANGE`: Check range [lo, hi]
  - `TPB_PARM_LIST`: Check against list
  - `TPB_PARM_CUSTOM`: Custom check function
- **TTTT** (bits 0-15): Type code
  - `TPB_INT8_T`, `TPB_INT16_T`, `TPB_INT32_T`, `TPB_INT64_T`
  - `TPB_UINT8_T`, `TPB_UINT16_T`, `TPB_UINT32_T`, `TPB_UINT64_T`
  - `TPB_FLOAT_T`, `TPB_DOUBLE_T`, `TPB_LONG_DOUBLE_T`
  - `TPB_STRING_T`, `TPB_CHAR_T`

**Variable Arguments:**
- **TPB_PARM_RANGE**: Two args (lo, hi)
  - Signed/unsigned int: int64_t/uint64_t lo, hi
  - Float types: double lo, hi
- **TPB_PARM_LIST**: Two args (n, plist)
  - `n`: Number of valid values (int)
  - `plist`: Pointer to array of valid values
- **TPB_PARM_NOCHECK**: No additional arguments

**Returns:**
- `0` on success
- Error code otherwise

**Example:**
```c
// Range check for integer
tpb_k_add_parm("ntest", "Number of tests", "10",
               TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_RANGE,
               (int64_t)1, (int64_t)10000);

// List check for string
const char *dtypes[] = {"float", "double", "int"};
tpb_k_add_parm("dtype", "Data type", "double",
               TPB_PARM_CLI | TPB_STRING_T | TPB_PARM_LIST,
               3, dtypes);

// No check for double
tpb_k_add_parm("epsilon", "Convergence threshold", "1e-6",
               TPB_PARM_CLI | TPB_DOUBLE_T | TPB_PARM_NOCHECK);
```

---

### `tpb_k_add_runner` (Deprecated)

Deprecated compatibility wrapper. Ignores the runner pointer and calls
`tpb_k_finalize_pli()` internally. Kept so that existing kernel sources
compile without changes.

```c
int tpb_k_add_runner(int (*runner)(void));
```

**Parameters:**
- `runner`: Function pointer (ignored)

**Returns:**
- `0` on success
- Error code otherwise

---

### `tpb_k_add_output`

Register a new output data definition for the current kernel.

```c
int tpb_k_add_output(const char *name, const char *note, 
                     TPB_DTYPE dtype, TPB_UNIT_T unit);
```

**Parameters:**
- `name`: Output name (used to look up when allocating/reporting)
- `note`: Human-readable description
- `dtype`: Data type of the output (TPB_INT64_T, TPB_DOUBLE_T, etc.)
- `unit`: Unit type (TPB_UNIT_NS, TPB_UNIT_BYTE, etc.)

**Returns:**
- `0` on success
- Error code otherwise

**Note:** Must be called during kernel registration (after `tpb_k_register`, before `tpb_k_finalize_pli`).

---

### `tpb_k_get_arg`

Get argument value from runtime handle.

```c
int tpb_k_get_arg(const char *name, TPB_DTYPE dtype, void *argptr);
```

**Parameters:**
- `name`: Parameter name
- `dtype`: Data type of the parameter
- `argptr`: Pointer to receive the argument value

**Returns:**
- Pointer to parameter value
- NULL if not found

---

### `tpb_k_get_timer`

Get timer function.

```c
int tpb_k_get_timer(tpb_timer_t *timer);
```

**Parameters:**
- `timer`: Pointer to receive timer structure

**Returns:**
- Error code (0 on success)

---

### `tpb_k_alloc_output`

Allocate memory for output data in the TPB framework.

```c
int tpb_k_alloc_output(const char *name, uint64_t n, void *ptr);
```

**Parameters:**
- `name`: The name of an output variable
- `n`: The number of elements with dtype defined in `tpb_k_add_output`
- `ptr`: Pointer to the header of allocated memory (NULL if failed)

**Returns:**
- `0` if successful
- Error code otherwise

---

## Timer Functions

Timer functions for precise timing measurements. Defined in `timers/timers.h`.

### Available Timers

| Timer Name | Platform | Description |
|------------|----------|-------------|
| `TIMER_CLOCK_GETTIME` | All | POSIX clock_gettime timer |
| `TIMER_TSC_ASYM` | x86_64 | Time Stamp Counter timer |

---

### Timer Structure Functions

Each timer implements the following function pointers:

```c
// Timer initialization
int init_timer_clock_gettime(void);

// Start timing
void tick_clock_gettime(int64_t *ts);

// Stop timing
void tock_clock_gettime(int64_t *ts);

// Get current timestamp
void get_time_clock_gettime(int64_t *ts);
```

**Parameters:**
- `ts`: Pointer to int64_t to receive/store timestamp

**Usage:**
1. Call `init_*()` to initialize timer
2. Call `tick_*(&start_ts)` to record start time
3. Execute benchmark code
4. Call `tock_*(&end_ts)` to record end time
5. Use `get_time_*()` to get current timestamp if needed

---

## Statistics Functions

Statistical analysis functions for benchmark data. Defined in `tpb-stat.h`.

### `tpb_stat_qtile_1d`

Calculate quantiles from a 1D array of generic data type.

```c
int tpb_stat_qtile_1d(void *arr, size_t narr, TPB_DTYPE dtype,
                      double *qarr, size_t nq, double *qout);
```

**Parameters:**
- `arr`: Pointer to input 1D array of data
- `narr`: Number of elements in the input array
- `dtype`: Data type of the input array elements
- `qarr`: Pointer to array of quantile positions (values in [0.0, 1.0])
- `nq`: Number of quantile positions in qarr
- `qout`: Pointer to output array for calculated quantile values

**Returns:**
- `TPBE_SUCCESS` on success
- Error code otherwise

---

### `tpb_stat_mean`

Calculate the arithmetic mean of a 1D array.

```c
int tpb_stat_mean(void *arr, size_t narr, TPB_DTYPE dtype, double *mean_out);
```

**Parameters:**
- `arr`: Pointer to input 1D array of data
- `narr`: Number of elements in the input array
- `dtype`: Data type of the input array elements
- `mean_out`: Pointer to output double for the mean

**Returns:**
- `TPBE_SUCCESS` on success
- Error code otherwise

---

### `tpb_stat_max`

Find the maximum value in a 1D array.

```c
int tpb_stat_max(void *arr, size_t narr, TPB_DTYPE dtype, double *max_out);
```

**Parameters:**
- `arr`: Pointer to input 1D array of data
- `narr`: Number of elements in the input array
- `dtype`: Data type of the input array elements
- `max_out`: Pointer to output double for the maximum

**Returns:**
- `TPBE_SUCCESS` on success
- Error code otherwise

---

### `tpb_stat_min`

Find the minimum value in a 1D array.

```c
int tpb_stat_min(void *arr, size_t narr, TPB_DTYPE dtype, double *min_out);
```

**Parameters:**
- `arr`: Pointer to input 1D array of data
- `narr`: Number of elements in the input array
- `dtype`: Data type of the input array elements
- `min_out`: Pointer to output double for the minimum

**Returns:**
- `TPBE_SUCCESS` on success
- Error code otherwise

---

## Unit Conversion Functions

Functions for converting and formatting units. Defined in `tpb-unitcast.h`.

### `tpb_unit_get_scale`

Get scale factor to convert from base unit to given unit.

```c
double tpb_unit_get_scale(TPB_UNIT_T unit);
```

**Parameters:**
- `unit`: The target unit

**Returns:**
- Scale factor (divide base unit value by this to get target unit value)

**Note:**
- For EXP-based units: scale = base^exponent
- For MUL-based units: scale = multiplier value

---

### `tpb_cast_unit`

Cast array values to appropriate unit for human readability.

```c
int tpb_cast_unit(void *arr, int narr, TPB_DTYPE dtype,
                  TPB_UNIT_T unit_current, TPB_UNIT_T *unit_cast,
                  double *arr_cast, int sigbit, int decbit);
```

**Parameters:**
- `arr`: Input array (any TPB_DTYPE)
- `narr`: Number of elements
- `dtype`: Data type of input array
- `unit_current`: Current unit of the values
- `unit_cast`: Output: the unit after casting
- `arr_cast`: Output: cast values as double array (must be pre-allocated)
- `sigbit`: Target significant figures
- `decbit`: Maximum decimal places

**Returns:**
- `TPBE_SUCCESS` or error code

---

### `tpb_format_scientific`

Format value with scientific notation.

```c
int tpb_format_scientific(double value, char *buf, size_t bufsize,
                          int sigbit, int intbit);
```

**Parameters:**
- `value`: The value to format
- `buf`: Output buffer
- `bufsize`: Buffer size
- `sigbit`: Total significant figures
- `intbit`: Integer digits before decimal point

**Returns:**
- Number of characters written

---

### `tpb_format_value`

Format value according to sigbit/intbit rules.

```c
int tpb_format_value(double value, char *buf, size_t bufsize,
                     int sigbit, int intbit);
```

**Parameters:**
- `value`: The value to format
- `buf`: Output buffer
- `bufsize`: Buffer size
- `sigbit`: Total significant figures (0 or negative = no limit)
- `intbit`: Integer digits before decimal (0 or negative = no limit)

**Returns:**
- Number of characters written

---

### `tpb_unit_to_string`

Convert TPB_UNIT_T to human-readable string.

```c
const char *tpb_unit_to_string(TPB_UNIT_T unit);
```

**Parameters:**
- `unit`: The unit code

**Returns:**
- Pointer to static string representation

---

## Error Handling

Functions for error handling and validation. Defined in `tpb-impl.h`.

### `tpb_get_err_exit_flag`

Get error exit flag from error code.

```c
int tpb_get_err_exit_flag(int err);
```

**Parameters:**
- `err`: Error code

**Returns:**
- Error type flag

---

### `tpb_get_err_msg`

Get error message from error code.

```c
const char *tpb_get_err_msg(int err);
```

**Parameters:**
- `err`: Error code

**Returns:**
- Error message string

---

### `tpb_char_is_legal_int`

Verify a string represents an integer within [lower, upper].

```c
int tpb_char_is_legal_int(int64_t lower, int64_t upper, char *str);
```

**Parameters:**
- `lower`: Lower bound
- `upper`: Upper bound
- `str`: String to verify

**Returns:**
- `1` if legal
- `0` otherwise

---

### `tpb_char_is_legal_fp`

Verify a string represents a floating point within [lower, upper].

```c
int tpb_char_is_legal_fp(double lower, double upper, char *str);
```

**Parameters:**
- `lower`: Lower bound
- `upper`: Upper bound
- `str`: String to verify

**Returns:**
- `1` if legal
- `0` otherwise

---

## Record Database API (rafdb)

Functions for workspace management, record entry operations, and record file I/O. Declared in `tpb-public.h`.

### Workspace Management

#### `tpb_raf_resolve_workspace`

Resolve the current TPBench workspace path using the following priority:
1. `$TPB_WORKSPACE` environment variable (if set and non-empty)
2. `$HOME/.tpbench/` if `etc/config.json` exists with a `"name"` field
3. Create default workspace at `$HOME/.tpbench/` with `etc/config.json` and rafdb subdirectories

```c
int tpb_raf_resolve_workspace(char *out_path, size_t pathlen);
```

**Parameters:**
- `out_path`: Buffer to receive the resolved workspace path
- `pathlen`: Size of the output buffer (should be at least `PATH_MAX`)

**Returns:**
- `TPBE_SUCCESS` (0) on success
- `TPBE_NULLPTR_ARG` if arguments are NULL
- `TPBE_FILE_IO_FAIL` on file system errors

---

#### `tpb_raf_init_workspace`

Initialize workspace directory structure. Creates `etc/config.json` and `rafdb/{task_batch,kernel,task}/` directories.

```c
int tpb_raf_init_workspace(const char *workspace_path);
```

**Parameters:**
- `workspace_path`: Root workspace directory path

**Returns:**
- `TPBE_SUCCESS` (0) on success or if already initialized
- `TPBE_NULLPTR_ARG` if path is NULL
- `TPBE_FILE_IO_FAIL` on directory creation errors

---

### Magic Signature Operations

#### `tpb_raf_build_magic`

Construct an 8-byte TPBench magic signature.

```c
void tpb_raf_build_magic(uint8_t ftype, uint8_t domain,
                           uint8_t pos, unsigned char out[8]);
```

**Parameters:**
- `ftype`: File type (`TPB_RAF_FTYPE_ENTRY=0xE0` or `TPB_RAF_FTYPE_RECORD=0xD0`)
- `domain`: Domain (`TPB_RAF_DOM_TBATCH=0`, `TPB_RAF_DOM_KERNEL=1`, `TPB_RAF_DOM_TASK=2`)
- `pos`: Position mark (`TPB_RAF_POS_START=0x53`, `TPB_RAF_POS_SPLIT=0x44`, `TPB_RAF_POS_END=0x45`)
- `out`: Output buffer (8 bytes)

**Magic Format:** `E1 54 50 42 <X> <Y> 31 E0` where X = ftype|domain, Y = position

---

#### `tpb_raf_validate_magic`

Validate an 8-byte magic signature against expected values.

```c
int tpb_raf_validate_magic(const unsigned char magic[8],
                             uint8_t ftype, uint8_t domain,
                             uint8_t pos);
```

**Parameters:**
- `magic`: 8-byte magic signature to validate
- `ftype`: Expected file type
- `domain`: Expected domain
- `pos`: Expected position

**Returns:**
- `1` if valid
- `0` if invalid

---

#### `tpb_raf_magic_scan`

Scan a buffer for TPBench magic signatures.

```c
int tpb_raf_magic_scan(const void *buf, size_t len,
                         size_t *offsets, int *nfound,
                         int max_results);
```

**Parameters:**
- `buf`: Buffer to scan
- `len`: Buffer length in bytes
- `offsets`: Output array for found magic offsets (must be pre-allocated)
- `nfound`: Output: total number of magic signatures found
- `max_results`: Maximum results to store in `offsets`

**Returns:**
- `TPBE_SUCCESS` on success
- `TPBE_NULLPTR_ARG` if required arguments are NULL

---

### Entry Operations (.tpbe)

Entry files store fixed-size record summaries for fast indexing (`tbatch_entry_t` / `kernel_entry_t`: 264 bytes; `task_entry_t`: 232 bytes). The public macro `TPB_RAF_RESERVE_SIZE` (128) is the tail reserve size in each entry and the opaque reserve block size in each `.tpbr` meta section.

#### `tpb_raf_entry_append_tbatch`

Append a tbatch entry to `rafdb/task_batch/task_batch.tpbe`.

```c
int tpb_raf_entry_append_tbatch(const char *workspace,
                                  const tbatch_entry_t *entry);
```

**Parameters:**
- `workspace`: Workspace root path
- `entry`: Pointer to `tbatch_entry_t` (264 bytes on disk)

**Returns:**
- `TPBE_SUCCESS` on success
- `TPBE_NULLPTR_ARG` if arguments are NULL
- `TPBE_FILE_IO_FAIL` on file errors

---

#### `tpb_raf_entry_append_kernel`

Append a kernel entry to `rafdb/kernel/kernel.tpbe`.

```c
int tpb_raf_entry_append_kernel(const char *workspace,
                                  const kernel_entry_t *entry);
```

**Parameters:**
- `workspace`: Workspace root path
- `entry`: Pointer to `kernel_entry_t` (264 bytes on disk)

---

#### `tpb_raf_entry_append_task`

Append a task entry to `rafdb/task/task.tpbe`.

```c
int tpb_raf_entry_append_task(const char *workspace,
                                const task_entry_t *entry);
```

**Parameters:**
- `workspace`: Workspace root path
- `entry`: Pointer to `task_entry_t` (232 bytes on disk)

---

#### `tpb_raf_entry_list_tbatch`

List all tbatch entries from the .tpbe file.

```c
int tpb_raf_entry_list_tbatch(const char *workspace,
                                tbatch_entry_t **entries,
                                int *count);
```

**Parameters:**
- `workspace`: Workspace root path
- `entries`: Output: allocated array of entries (caller must `free()`)
- `count`: Output: number of entries

**Returns:**
- `TPBE_SUCCESS` on success (even if no entries)
- `TPBE_NULLPTR_ARG` if arguments are NULL
- `TPBE_FILE_IO_FAIL` on file errors
- `TPBE_MALLOC_FAIL` on allocation failure

**Note:** Returns empty list (entries=NULL, count=0) if file doesn't exist.

---

#### `tpb_raf_entry_list_kernel`

List all kernel entries from `rafdb/kernel/kernel.tpbe`.

```c
int tpb_raf_entry_list_kernel(const char *workspace,
                                kernel_entry_t **entries,
                                int *count);
```

---

#### `tpb_raf_entry_list_task`

List all task entries from `rafdb/task/task.tpbe`.

```c
int tpb_raf_entry_list_task(const char *workspace,
                              task_entry_t **entries,
                              int *count);
```

---

### Record Operations (.tpbr)

Record files store full attributes, headers, and data for each record.

#### `tpb_raf_record_write_tbatch`

Write a complete tbatch record to `<TBatchID>.tpbr`.

```c
int tpb_raf_record_write_tbatch(const char *workspace,
                                  const tbatch_attr_t *attr,
                                  const void *data,
                                  uint64_t datasize);
```

**Parameters:**
- `workspace`: Workspace root path
- `attr`: TBatch attributes with populated `headers` array
- `data`: Record data buffer (may be NULL if `datasize==0`)
- `datasize`: Size of data in bytes

**Returns:**
- `TPBE_SUCCESS` on success
- `TPBE_NULLPTR_ARG` if required arguments are NULL
- `TPBE_FILE_IO_FAIL` on write errors

---

#### `tpb_raf_record_read_tbatch`

Read a tbatch record from `<TBatchID>.tpbr`.

```c
int tpb_raf_record_read_tbatch(const char *workspace,
                                 const unsigned char tbatch_id[20],
                                 tbatch_attr_t *attr,
                                 void **data,
                                 uint64_t *datasize);
```

**Parameters:**
- `workspace`: Workspace root path
- `tbatch_id`: 20-byte TBatchID
- `attr`: Output attributes (headers allocated internally)
- `data`: Output data buffer (caller must `free()`, may be NULL)
- `datasize`: Output data size

**Returns:**
- `TPBE_SUCCESS` on success
- `TPBE_FILE_IO_FAIL` on read errors
- `TPBE_MALLOC_FAIL` on allocation failure

**Note:** Use `tpb_raf_free_headers()` to free `attr->headers`.

---

#### `tpb_raf_record_write_kernel`

Write a kernel record to `<KernelID>.tpbr`.

```c
int tpb_raf_record_write_kernel(const char *workspace,
                                  const kernel_attr_t *attr,
                                  const void *data,
                                  uint64_t datasize);
```

---

#### `tpb_raf_record_read_kernel`

Read a kernel record from `<KernelID>.tpbr`.

```c
int tpb_raf_record_read_kernel(const char *workspace,
                                 const unsigned char kernel_id[20],
                                 kernel_attr_t *attr,
                                 void **data,
                                 uint64_t *datasize);
```

---

#### `tpb_raf_record_write_task`

Write a task record to `<TaskRecordID>.tpbr`.

```c
int tpb_raf_record_write_task(const char *workspace,
                                const task_attr_t *attr,
                                const void *data,
                                uint64_t datasize);
```

---

#### `tpb_raf_record_read_task`

Read a task record from `<TaskRecordID>.tpbr`.

```c
int tpb_raf_record_read_task(const char *workspace,
                               const unsigned char task_id[20],
                               task_attr_t *attr,
                               void **data,
                               uint64_t *datasize);
```

---

#### `tpb_raf_free_headers`

Free header array allocated by record read functions.

```c
void tpb_raf_free_headers(tpb_meta_header_t *headers,
                            uint32_t nheader);
```

**Parameters:**
- `headers`: Header array (may be NULL, in which case no-op)
- `nheader`: Number of headers

---

### ID Generation

All IDs are 20-byte SHA1 hashes. Use `tpb_raf_id_to_hex()` to convert to 40-char hex string.

#### `tpb_raf_gen_tbatch_id`

Generate TBatchID from execution context.

```c
int tpb_raf_gen_tbatch_id(tpb_dtbits_t utc_bits,
                            uint64_t btime,
                            const char *hostname,
                            const char *username,
                            uint32_t front_pid,
                            unsigned char id_out[20]);
```

**Formula:** `SHA1("tbatch" + utc_bits + btime + hostname + username + front_pid)`

---

#### `tpb_raf_gen_kernel_id`

Generate KernelID from kernel artifacts.

```c
int tpb_raf_gen_kernel_id(const char *kernel_name,
                            const unsigned char so_sha1[20],
                            const unsigned char bin_sha1[20],
                            unsigned char id_out[20]);
```

**Formula:** `SHA1("kernel" + kernel_name + so_sha1 + bin_sha1)`

---

#### `tpb_raf_gen_task_id`

Generate TaskRecordID from invocation context.

```c
int tpb_raf_gen_task_id(tpb_dtbits_t utc_bits,
                          uint64_t btime,
                          const char *hostname,
                          const char *username,
                          const unsigned char tbatch_id[20],
                          const unsigned char kernel_id[20],
                          uint32_t order,
                          uint32_t pid,
                          uint32_t tid,
                          unsigned char id_out[20]);
```

**Formula:** `SHA1("task" + utc_bits + btime + hostname + username + tbatch_id + kernel_id + order + pid + tid)`

---

#### `tpb_raf_id_to_hex`

Convert 20-byte ID to 40-character hex string.

```c
void tpb_raf_id_to_hex(const unsigned char id[20],
                         char hex[41]);
```

**Parameters:**
- `id`: 20-byte binary ID
- `hex`: Output buffer (41 bytes: 40 hex chars + null terminator)

---

### Record Data Types

#### `tpb_dtbits_t`

64-bit compact datetime representation.

```c
typedef uint64_t tpb_dtbits_t;
```

Bit layout:
- Bits 0-5: seconds (0-59)
- Bits 6-11: minutes (0-59)
- Bits 12-16: hours (0-23)
- Bits 17-21: day (1-31)
- Bits 22-25: month (1-12)
- Bits 26-33: year bias from 1970 (0-255)
- Bits 34-41: timezone in 15-min increments
- Bits 42-63: reserved

Use `tpb_ts_bits_to_isoutc()` to convert to ISO 8601 string.

---

#### `tpb_meta_header_t`

Metadata header describing record data layout. Dimensions use fixed-width inline arrays.

```c
typedef struct tpb_meta_header {
    uint32_t block_size;                      // Total header size on disk (2840)
    uint32_t ndim;                            // Number of dimensions [0, TPBM_DATA_NDIM_MAX]
    uint64_t data_size;                       // Record data size in bytes
    uint32_t type_bits;                       // Data type control bits: SOURCE|CHECK|TYPE
    uint32_t _reserve;                        // Reserved padding for alignment
    uint64_t uattr_bits;                      // Metric unit encoding (TPB_UNIT_T)
    char name[TPBM_NAME_STR_MAX_LEN];        // Header name (256 bytes)
    char note[TPBM_NOTE_STR_MAX_LEN];        // Description (2048 bytes)
    uint64_t dimsizes[TPBM_DATA_NDIM_MAX];   // Dimension sizes (7 slots, 56 bytes)
    char dimnames[TPBM_DATA_NDIM_MAX][64];   // Dimension names (7 x 64 bytes = 448 bytes)
} tpb_meta_header_t;
```

**On-disk size:** 2840 bytes (fixed)

---

#### Entry Structures

**`tbatch_entry_t`** (264 bytes) - Slim tbatch summary:

```c
typedef struct tbatch_entry {
    unsigned char tbatch_id[20];    // TBatchID
    unsigned char inherit_from[20];     // Lineage: source TBatchID or zero
    tpb_dtbits_t start_utc_bits;    // Start datetime
    uint64_t duration;              // Duration in nanoseconds
    char hostname[64];              // Hostname
    uint32_t nkernel;               // Number of kernels
    uint32_t ntask;                 // Task entry points (derive_to==0)
    uint32_t nscore;                // Number of scores (always 0 for now)
    uint32_t batch_type;            // 0=run, 1=benchmark
    unsigned char reserve[TPB_RAF_RESERVE_SIZE]; // Reserved (128)
} tbatch_entry_t;
```

**`kernel_entry_t`** (264 bytes) - Slim kernel summary:

```c
typedef struct kernel_entry {
    unsigned char kernel_id[20];    // KernelID
    unsigned char inherit_from[20];     // Lineage: source KernelID or zero
    char kernel_name[64];           // Kernel name
    unsigned char so_sha1[20];    // Shared library SHA1
    uint32_t kctrl;                 // Kernel control bits
    uint32_t nparm;                 // Number of parameters
    uint32_t nmetric;               // Number of metrics
    unsigned char reserve[TPB_RAF_RESERVE_SIZE]; // Reserved (128)
} kernel_entry_t;
```

**`task_entry_t`** (232 bytes) - Slim task summary:

```c
typedef struct task_entry {
    unsigned char task_record_id[20];   // TaskRecordID
    unsigned char inherit_from[20];     // Lineage: source TaskRecordID or zero
    unsigned char tbatch_id[20];        // TBatchID
    unsigned char kernel_id[20];        // KernelID
    tpb_dtbits_t utc_bits;              // Invocation datetime
    uint64_t duration;                  // Duration (ns)
    uint32_t exit_code;                 // Exit code
    uint32_t handle_index;              // Handle index
    unsigned char derive_to[20];        // Merge/capsule target, or zero
    unsigned char reserve[TPB_RAF_RESERVE_SIZE - 20]; // Reserved (108)
} task_entry_t;
```

---

#### Attribute Structures

**`tbatch_attr_t`** - Full tbatch attributes for .tpbr:

```c
typedef struct tbatch_attr {
    unsigned char tbatch_id[20];    // Primary Link ID
    unsigned char derive_to[20];       // Derivation target, or zero
    unsigned char inherit_from[20];     // Lineage / provenance
    tpb_dtbits_t utc_bits;          // Start datetime
    uint64_t btime;                 // Boot time (ns)
    uint64_t duration;              // Duration (ns)
    char hostname[64];              // Hostname
    char username[64];              // Username
    uint32_t front_pid;             // Front-end PID
    uint32_t nkernel;               // Kernel count
    uint32_t ntask;                 // Task entry points (derive_to==0)
    uint32_t nscore;                // Score count
    uint32_t batch_type;            // 0=run, 1=benchmark
    uint32_t nheader;               // Number of headers
    tpb_meta_header_t *headers;     // Header array
} tbatch_attr_t;
```

**`kernel_attr_t`** - Full kernel attributes:

```c
typedef struct kernel_attr {
    unsigned char kernel_id[20];    // KernelID
    unsigned char derive_to[20];       // Derivation target, or zero
    unsigned char inherit_from[20];     // Lineage / provenance
    unsigned char src_sha1[20];     // Source SHA1
    unsigned char so_sha1[20];     // Shared library SHA1
    unsigned char bin_sha1[20];    // Executable SHA1
    char kernel_name[256];          // Kernel name
    char version[64];               // Version string
    char description[2048];          // Description
    uint32_t nparm;                 // Parameter count
    uint32_t nmetric;               // Metric count
    uint32_t kctrl;                 // Control bits (PLI=2)
    uint32_t nheader;               // Header count
    uint32_t reserve;               // Padding
    tpb_meta_header_t *headers;     // Header array
} kernel_attr_t;
```

**`task_attr_t`** - Full task attributes:

```c
typedef struct task_attr {
    unsigned char task_record_id[20];   // TaskRecordID
    unsigned char derive_to[20];           // Derivation target, or zero
    unsigned char inherit_from[20];         // Lineage / provenance
    unsigned char tbatch_id[20];        // Foreign key: TBatchID
    unsigned char kernel_id[20];        // Foreign key: KernelID
    tpb_dtbits_t utc_bits;              // Invocation datetime
    uint64_t btime;                     // Boot time (ns)
    uint64_t duration;                  // Duration (ns)
    uint32_t exit_code;                 // Exit code
    uint32_t handle_index;              // Handle index
    uint32_t pid;                       // Writer process ID
    uint32_t tid;                       // Writer thread ID
    uint32_t ninput;                    // Input headers count
    uint32_t noutput;                   // Output headers count
    uint32_t nheader;                   // Total headers
    uint32_t reserve;                   // Padding
    tpb_meta_header_t *headers;         // Header array
} task_attr_t;
```

---

## Constants

### String Length Limits

| Constant | Value | Description |
|----------|-------|-------------|
| `TPBM_CLI_STR_MAX_LEN` | 4096 | Maximum CLI string length |
| `TPBM_NAME_STR_MAX_LEN` | 256 | Maximum name string length |
| `TPBM_NOTE_STR_MAX_LEN` | 2048 | Maximum note string length |
| `TPBM_CLI_K_MAX` | 128 | Maximum number of kernels |
| `TPB_RAF_RESERVE_SIZE` | 128 | `.tpbr` meta reserve and `.tpbe` entry tail (bytes) |

---

## Usage Example

```c
#include "tpb-driver.h"
#include "tpb-io.h"

// Kernel runner function
int my_kernel_runner(void) {
    // Get parameter values
    int64_t ntest;
    tpb_k_get_arg("ntest", TPB_INT64_T, &ntest);
    
    // Allocate output memory
    double *results;
    tpb_k_alloc_output("time", ntest, &results);
    
    // Run benchmark
    for (int i = 0; i < ntest; i++) {
        // Benchmark code here
        results[i] = /* measured value */;
    }
    
    return TPBE_SUCCESS;
}

// Register kernel
void register_my_kernel(void) {
    // Register kernel
    tpb_k_register("my_kernel", "Example benchmark kernel");
    
    // Add parameters
    tpb_k_add_parm("ntest", "Number of tests", "100",
                   TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_RANGE,
                   (int64_t)1, (int64_t)1000000);
    
    // Add output
    tpb_k_add_output("time", "Execution time", TPB_DOUBLE_T, TPB_UNIT_NS);
    
    // Finalize registration
    tpb_k_finalize_pli();
}

int main(void) {
    // Initialize TPBench
    tpb_register_kernel();
    register_my_kernel();
    
    // Set timer
    tpb_timer_t timer;
    timer.tick = tick_clock_gettime;
    timer.tock = tock_clock_gettime;
    tpb_driver_set_timer(timer);
    
    // Run benchmark
    tpb_driver_add_handle("my_kernel");
    tpb_driver_run_all();
    
    // Output results
    tpb_cliout_results(/* handle */);
    
    // Cleanup
    tpb_driver_clean_handle(/* handle */);
    
    return 0;
}
```

---

## Thread Safety

TPBench library functions are generally not thread-safe. Concurrent calls from multiple threads must be synchronized by the caller.

---

## Building

To link against libtpbench.so:

```bash
gcc -o my_benchmark my_benchmark.c -L/path/to/lib -ltpbench -lm
```

---

## Additional Information

- **Project Version:** 0.8
- **Required C Standard:** C11
- **Dependencies:** libm (math library)
- **Platform Support:** Linux (x86_64, AMD64)

For more information, see:
- Source files in `/src/` directory
- CMakeLists.txt for build configuration