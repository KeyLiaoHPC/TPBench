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
    tpb_parm_value_t default_value;          // Default value
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
    int (*k_run)(void);                         // Runner function
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

---

### `tpb_list`

List all registered kernels to stdout.

```c
void tpb_list(void);
```

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

### `tpb_get_kernel_count`

Get the number of registered kernels.

```c
int tpb_get_kernel_count(void);
```

**Returns:**
- Kernel count

---

### `tpb_get_kernel`

Get a registered kernel by name.

```c
int tpb_get_kernel(const char *name, tpb_kernel_t **kernel_out);
```

**Parameters:**
- `name`: Kernel name
- `kernel_out`: Pointer to receive kernel address

**Returns:**
- `0` on success
- Error code otherwise

---

### `tpb_get_kernel_by_index`

Get a registered kernel by index.

```c
int tpb_get_kernel_by_index(int idx, tpb_kernel_t **kernel_out);
```

**Parameters:**
- `idx`: Kernel index
- `kernel_out`: Pointer to receive kernel address

**Returns:**
- `0` on success
- Error code otherwise

---

### `tpb_run_kernel`

Run a registered kernel.

```c
int tpb_run_kernel(tpb_k_rthdl_t *handle);
```

**Parameters:**
- `handle`: Runtime handle with kernel info, timer, parms, and result package

**Returns:**
- Error code (0 on success)

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
                   const char *default_value, TPB_DTYPE dtype, ...);
```

**Parameters:**
- `name`: Parameter name (used for CLI argument matching)
- `note`: Human-readable parameter description
- `default_value`: String representation of default value
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

### `tpb_k_add_runner`

Set the runner function for the current kernel.

```c
int tpb_k_add_runner(int (*runner)(void));
```

**Parameters:**
- `runner`: Function pointer to kernel runner

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

**Note:** Must be called during kernel registration (after `tpb_k_register`, before `tpb_k_add_runner`).

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

## Constants

### String Length Limits

| Constant | Value | Description |
|----------|-------|-------------|
| `TPBM_CLI_STR_MAX_LEN` | 4096 | Maximum CLI string length |
| `TPBM_NAME_STR_MAX_LEN` | 256 | Maximum name string length |
| `TPBM_NOTE_STR_MAX_LEN` | 2048 | Maximum note string length |
| `TPBM_CLI_K_MAX` | 128 | Maximum number of kernels |

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
    
    // Set runner
    tpb_k_add_runner(my_kernel_runner);
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