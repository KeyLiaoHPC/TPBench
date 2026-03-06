# TPBench Core Library Design Document: Kernel Parameter and Metric Attribute Definitions

文档语言： [中文](docs/design_kernel_args_metrics_attribute_CN.md) | [English](docs/design_args_metrics_attribute_EN.md)

Doc languages:  [中文](docs/design_kernel_args_metrics_attribute_CN.md) |
[English](docs/design_args_metrics_attribute_EN.md) 

## 1 Input Parameter Attributes


### 1.1 Parameter Control Bits Layout

The `ctrlbits` field of input parameters uses a 32-bit encoding, divided into three regions:

```
+--------+--------+------------------+
| 24-31  | 16-23  |      0-15        |
| Source | Check  |      Type        |
+--------+--------+------------------+
```

### 1.2 Source Flags (bits 24-31)

| Macro | Value | Description |
|-------|-------|-------------|
| [`TPB_PARM_CLI`](src/include/tpb-public.h:40) | `0x01000000` | Parameter can be passed from command line |
| [`TPB_PARM_MACRO`](src/include/tpb-public.h:41) | `0x02000000` | Parameter can be passed from compile-time macro |
| [`TPB_PARM_FILE`](src/include/tpb-public.h:42) | `0x04000000` | Parameter can be passed from configuration file |
| [`TPB_PARM_ENV`](src/include/tpb-public.h:43) | `0x08000000` | Parameter can be passed from environment variable |

### 1.3 Check/Validation Flags (bits 16-23)

| Macro | Value | Description |
|-------|-------|-------------|
| [`TPB_PARM_NOCHECK`](src/include/tpb-public.h:47) | `0x00000000` | No validation |
| [`TPB_PARM_RANGE`](src/include/tpb-public.h:48) | `0x00010000` | Range validation (requires lo, hi bounds) |
| [`TPB_PARM_LIST`](src/include/tpb-public.h:49) | `0x00020000` | List validation (requires n, plist of valid values) |
| [`TPB_PARM_CUSTOM`](src/include/tpb-public.h:50) | `0x00040000` | Custom validation function |

### 1.4 Type Flags (bits 0-15)

| Macro | Value | C Type | Description |
|-------|-------|--------|-------------|
| [`TPB_INT8_T`](src/include/tpb-public.h:55) | `0x00000137` | `int8_t` | 8-bit signed integer |
| [`TPB_INT16_T`](src/include/tpb-public.h:56) | `0x00000238` | `int16_t` | 16-bit signed integer |
| [`TPB_INT32_T`](src/include/tpb-public.h:57) | `0x00000439` | `int32_t` | 32-bit signed integer |
| [`TPB_INT64_T`](src/include/tpb-public.h:58) | `0x0000083a` | `int64_t` | 64-bit signed integer |
| [`TPB_UINT8_T`](src/include/tpb-public.h:59) | `0x0000013b` | `uint8_t` | 8-bit unsigned integer |
| [`TPB_UINT16_T`](src/include/tpb-public.h:60) | `0x0000023c` | `uint16_t` | 16-bit unsigned integer |
| [`TPB_UINT32_T`](src/include/tpb-public.h:61) | `0x0000043d` | `uint32_t` | 32-bit unsigned integer |
| [`TPB_UINT64_T`](src/include/tpb-public.h:62) | `0x0000083e` | `uint64_t` | 64-bit unsigned integer |
| [`TPB_FLOAT_T`](src/include/tpb-public.h:63) | `0x0000040a` | `float` | Single-precision float |
| [`TPB_DOUBLE_T`](src/include/tpb-public.h:64) | `0x0000080b` | `double` | Double-precision float |
| [`TPB_LONG_DOUBLE_T`](src/include/tpb-public.h:65) | `0x0000100c` | `long double` | Extended-precision float |
| [`TPB_CHAR_T`](src/include/tpb-public.h:66) | `0x00000101` | `char` | Single character |
| [`TPB_STRING_T`](src/include/tpb-public.h:67) | `0x00001000` | `char*` | String |
| [`TPB_DTYPE_TIMER_T`](src/include/tpb-public.h:68) | `0x0000083F` | `tpb_timer_t` | Timer type |

### 1.5 Runtime Parameter Structure ([`tpb_rt_parm_t`](src/include/tpb-public.h:120))

```c
typedef struct tpb_rt_parm {
    char name[TPBM_NAME_STR_MAX_LEN];      // Parameter name (max 256 chars)
    char note[TPBM_NOTE_STR_MAX_LEN];      // Parameter description (max 2048 chars)
    tpb_parm_value_t value;                // Current value
    tpb_parm_value_t default_value;        // Default value
    TPB_DTYPE ctrlbits;                    // Control bits (source|check|type)
    int nlims;                             // Number of limit values
    tpb_parm_value_t *plims;               // Limit value array (range bounds or list values)
} tpb_rt_parm_t;
```

### 1.6 Parameter Value Union ([`tpb_parm_value_t`](src/include/tpb-public.h:111))

```c
typedef union tpb_parm_value {
    int64_t i64;
    uint64_t u64;
    float f32;
    double f64;
    char c;
} tpb_parm_value_t;
```

---

## 2 Output Metric Attributes


### 2.1 Output Parameter Structure ([`tpb_k_output_t`](src/include/tpb-public.h:131))

```c
typedef struct tpb_k_output {
    char name[TPBM_NAME_STR_MAX_LEN];  // Output name (max 256 chars)
    char note[TPBM_NOTE_STR_MAX_LEN];  // Output description (max 2048 chars)
    TPB_DTYPE dtype;                   // Data type
    TPB_UNIT_T unit;                   // Unit encoding
    int n;                             // Number of elements (allocated at runtime)
    void *p;                           // Data pointer (allocated at runtime)
} tpb_k_output_t;
```

### 2.2 Unit Encoding Structure (64-bit)

```
+-------------+-------------+----------------+-----------+---------------------+
| 48-63       | 44-47       | 36-43          | 32-35     | 0-31                |
| Attribution | Unit Kind   | Unit Name Code | Base Type | Exponent/Multiplier |
+-------------+-------------+----------------+-----------+---------------------+
```

### 2.3 Attribution Control Bits (bits 48-63)

| Macro | Bit | Description |
|-------|-----|-------------|
| [`TPB_UATTR_CAST_MASK`](src/include/tpb-unitdefs.h:32) | bit 48 | Unit conversion control |
| [`TPB_UATTR_CAST_Y`](src/include/tpb-unitdefs.h:33) | `1<<48` | Enable automatic unit conversion |
| [`TPB_UATTR_CAST_N`](src/include/tpb-unitdefs.h:34) | `0<<48` | Disable automatic unit conversion |
| [`TPB_UATTR_SHAPE_MASK`](src/include/tpb-unitdefs.h:35) | bits 49-51 | Data shape (0-7 dimensional array) |
| [`TPB_UATTR_SHAPE_POINT`](src/include/tpb-unitdefs.h:36) | `0<<49` | Single-point data |
| [`TPB_UATTR_SHAPE_1D`](src/include/tpb-unitdefs.h:37) | `1<<49` | 1D array |
| [`TPB_UATTR_SHAPE_2D`](src/include/tpb-unitdefs.h:38) | `2<<49` | 2D array |
| [`TPB_UATTR_SHAPE_3D`](src/include/tpb-unitdefs.h:39) | `3<<49` | 3D array |
| [`TPB_UATTR_SHAPE_4D`](src/include/tpb-unitdefs.h:40) | `4<<49` | 4D array |
| [`TPB_UATTR_SHAPE_5D`](src/include/tpb-unitdefs.h:41) | `5<<49` | 5D array |
| [`TPB_UATTR_SHAPE_6D`](src/include/tpb-unitdefs.h:42) | `6<<49` | 6D array |
| [`TPB_UATTR_SHAPE_7D`](src/include/tpb-unitdefs.h:43) | `7<<49` | 7D array |
| [`TPB_UATTR_TRIM_MASK`](src/include/tpb-unitdefs.h:44) | bit 52 | Numeric trimming control |
| [`TPB_UATTR_TRIM_N`](src/include/tpb-unitdefs.h:45) | `1<<52` | Disable trimming |
| [`TPB_UATTR_TRIM_Y`](src/include/tpb-unitdefs.h:46) | `0<<52` | Enable trimming (default) |

### 2.4 Unit Kind (bits 44-47)

| Macro | Value | Description |
|-------|-------|-------------|
| [`TPB_UKIND_UNDEF`](src/include/tpb-unitdefs.h:50) | `0x0` | Undefined |
| [`TPB_UKIND_TIME`](src/include/tpb-unitdefs.h:51) | `0x1` | Time |
| [`TPB_UKIND_VOL`](src/include/tpb-unitdefs.h:52) | `0x2` | Volume/Capacity |
| [`TPB_UKIND_VOLPTIME`](src/include/tpb-unitdefs.h:53) | `0x3` | Rate (volume/time) |

### 2.5 Unit Name Code (bits 36-43)

#### Time Category (TIME)

| Macro | Description |
|-------|-------------|
| [`TPB_UNAME_WALLTIME`](src/include/tpb-unitdefs.h:56) | Wall-clock time |
| [`TPB_UNAME_PHYSTIME`](src/include/tpb-unitdefs.h:57) | Chip physical time |
| [`TPB_UNAME_DATETIME`](src/include/tpb-unitdefs.h:58) | Date time |
| [`TPB_UNAME_TICKTIME`](src/include/tpb-unitdefs.h:59) | Tick count |
| [`TPB_UNAME_TIMERTIME`](src/include/tpb-unitdefs.h:60) | Follow timer unit |

#### Volume Category (VOL)

| Macro | Description |
|-------|-------------|
| [`TPB_UNAME_DATASIZE`](src/include/tpb-unitdefs.h:61) | Data size |
| [`TPB_UNAME_OP`](src/include/tpb-unitdefs.h:62) | Operation count |
| [`TPB_UNAME_GRIDSIZE`](src/include/tpb-unitdefs.h:63) | Grid count |
| [`TPB_UNAME_BITSIZE`](src/include/tpb-unitdefs.h:64) | Binary bit size |

#### Rate Category (VOL/TIME)

| Macro | Description |
|-------|-------------|
| [`TPB_UNAME_OPS`](src/include/tpb-unitdefs.h:65) | Operations per second |
| [`TPB_UNAME_FLOPS`](src/include/tpb-unitdefs.h:66) | Floating-point operations per second |
| [`TPB_UNAME_TOKENPS`](src/include/tpb-unitdefs.h:67) | Tokens per second |
| [`TPB_UNAME_TPS`](src/include/tpb-unitdefs.h:68) | Transactions per second |
| [`TPB_UNAME_BITPS`](src/include/tpb-unitdefs.h:69) | Bits per second |
| [`TPB_UNAME_DATAPS`](src/include/tpb-unitdefs.h:70) | Bytes per second |
| [`TPB_UNAME_BITPCY`](src/include/tpb-unitdefs.h:71) | Bits per cycle |
| [`TPB_UNAME_DATAPCY`](src/include/tpb-unitdefs.h:72) | Bytes per cycle |
| [`TPB_UNAME_BITPTICK`](src/include/tpb-unitdefs.h:73) | Bits per tick |
| [`TPB_UNAME_DATAPTICK`](src/include/tpb-unitdefs.h:74) | Bytes per tick |

### 2.6 Base Type (bits 32-35)

| Macro | Description |
|-------|-------------|
| [`TPB_UBASE_BASE`](src/include/tpb-unitdefs.h:80) | Base unit |
| [`TPB_UBASE_BIN_EXP_P`](src/include/tpb-unitdefs.h:81) | Binary exponent (positive) |
| [`TPB_UBASE_BIN_EXP_N`](src/include/tpb-unitdefs.h:82) | Binary exponent (negative) |
| [`TPB_UBASE_BIN_MUL_P`](src/include/tpb-unitdefs.h:83) | Binary multiplier (positive) |
| [`TPB_UBASE_OCT_EXP_P`](src/include/tpb-unitdefs.h:84) | Octal exponent (positive) |
| [`TPB_UBASE_OCT_EXP_N`](src/include/tpb-unitdefs.h:85) | Octal exponent (negative) |
| [`TPB_UBASE_DEC_EXP_P`](src/include/tpb-unitdefs.h:86) | Decimal exponent (positive) |
| [`TPB_UBASE_DEC_EXP_N`](src/include/tpb-unitdefs.h:87) | Decimal exponent (negative) |
| [`TPB_UBASE_DEC_MUL_P`](src/include/tpb-unitdefs.h:88) | Decimal multiplier (positive) |
| [`TPB_UBASE_HEX_EXP_P`](src/include/tpb-unitdefs.h:89) | Hexadecimal exponent (positive) |
| [`TPB_UBASE_HEX_EXP_N`](src/include/tpb-unitdefs.h:90) | Hexadecimal exponent (negative) |

### 2.7 Common Unit Macros

#### Time Units

| Macro | Description |
|-------|-------------|
| [`TPB_UNIT_NS`](src/include/tpb-unitdefs.h:126) | Nanosecond |
| [`TPB_UNIT_US`](src/include/tpb-unitdefs.h:130) | Microsecond |
| [`TPB_UNIT_MS`](src/include/tpb-unitdefs.h:131) | Millisecond |
| [`TPB_UNIT_SS`](src/include/tpb-unitdefs.h:132) | Second |
| [`TPB_UNIT_CY`](src/include/tpb-unitdefs.h:134) | Cycle |

#### Data Volume Units

| Macro | Description |
|-------|-------------|
| [`TPB_UNIT_BYTE`](src/include/tpb-unitdefs.h:106) | Byte |
| [`TPB_UNIT_KIB`](src/include/tpb-unitdefs.h:107) | KiB (2^10) |
| [`TPB_UNIT_MIB`](src/include/tpb-unitdefs.h:108) | MiB (2^20) |
| [`TPB_UNIT_GIB`](src/include/tpb-unitdefs.h:109) | GiB (2^30) |
| [`TPB_UNIT_KB`](src/include/tpb-unitdefs.h:117) | KB (10^3) |
| [`TPB_UNIT_MB`](src/include/tpb-unitdefs.h:118) | MB (10^6) |
| [`TPB_UNIT_GB`](src/include/tpb-unitdefs.h:119) | GB (10^9) |

#### Performance Units

| Macro | Description |
|-------|-------------|
| [`TPB_UNIT_GFLOPS`](src/include/tpb-unitdefs.h:176) | GFLOPS (10^9) |
| [`TPB_UNIT_TFLOPS`](src/include/tpb-unitdefs.h:177) | TFLOPS (10^12) |
| [`TPB_UNIT_GBPS`](src/include/tpb-unitdefs.h:217) | GB/s |

---

## 3 Usage Examples


### 3.1 Adding Input Parameters

```c
// int64 parameter from command line, range validation [1, 100000]
tpb_k_add_parm("ntest", "Number of test iterations", "10",
               TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_RANGE,
               1, 100000);

// double parameter from command line, range validation [0, 10000]
tpb_k_add_parm("twarm", "Warm-up time in ms", "100",
               TPB_PARM_CLI | TPB_DOUBLE_T | TPB_PARM_RANGE,
               0, 10000);
```

### 3.2 Adding Output Metrics

```c
// Add time output, unit in nanoseconds
tpb_k_add_output("elapsed_time", "Elapsed wall-clock time",
                 TPB_INT64_T, TPB_UNIT_NS);

// Add performance output, unit in GFLOPS
tpb_k_add_output("performance", "Peak performance",
                 TPB_DOUBLE_T, TPB_UNIT_GFLOPS);

// Add data volume output, unit in GiB
tpb_k_add_output("data_size", "Total data processed",
                 TPB_UINT64_T, TPB_UNIT_GIB);
```

---

## 4 Attribute Summary Table


| Attribute Category | Input Parameter | Output Parameter |
|--------------------|-----------------|------------------|
| **Name** | `name` (256 chars) | `name` (256 chars) |
| **Description** | `note` (2048 chars) | `note` (2048 chars) |
| **Value/Data** | `value` + `default_value` | `p` (pointer) + `n` (count) |
| **Type** | `ctrlbits` (32-bit encoding) | `dtype` (data type) |
| **Validation** | `nlims` + `plims` | None |
| **Unit** | None | `unit` (64-bit encoding) |
| **Source** | Source flags (CLI/Macro/File/Env) | N/A |
| **Shape** | N/A | Shape flags (Point/1D-7D) |
| **Conversion** | N/A | Cast/Trim flags |
