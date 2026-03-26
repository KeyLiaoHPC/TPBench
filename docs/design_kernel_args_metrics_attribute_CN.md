# TPBench核心库设计文档：测试内核参数和指标属性定义

文档语言： [中文](docs/design_kernel_args_metrics_attribute_CN.md) | [English](docs/design_args_metrics_attribute_EN.md)

Doc languages:  [中文](docs/design_kernel_args_metrics_attribute_CN.md) |
[English](docs/design_args_metrics_attribute_EN.md) 

## 1 输入参数控制属性 (Input Parameter Attributes)

### 1.1 参数控制位结构 (Parameter Control Bits Layout)

输入参数的 `ctrlbits` 字段使用 32 位编码，分为三个区域：

```
+--------+--------+------------------+
| 24-31  | 16-23  |      0-15        |
| Source | Check  |      Type        |
+--------+--------+------------------+
```

### 1.2 参数来源标识 (Source Flags, bits 24-31)

| 宏定义 | 值 | 说明 |
|--------|-----|------|
| [`TPB_PARM_CLI`](src/include/tpb-public.h:40) | `0x01000000` | 参数可从命令行传入 |
| [`TPB_PARM_MACRO`](src/include/tpb-public.h:41) | `0x02000000` | 参数可从编译宏传入 |
| [`TPB_PARM_FILE`](src/include/tpb-public.h:42) | `0x04000000` | 参数可从配置文件传入 |
| [`TPB_PARM_ENV`](src/include/tpb-public.h:43) | `0x08000000` | 参数可从环境变量传入 |

### 1.3 参数校验模式 (Check/Validation Flags, bits 16-23)

| 宏定义 | 值 | 说明 |
|--------|-----|------|
| [`TPB_PARM_NOCHECK`](src/include/tpb-public.h:47) | `0x00000000` | 不进行校验 |
| [`TPB_PARM_RANGE`](src/include/tpb-public.h:48) | `0x00010000` | 范围校验 (需传入 lo, hi 边界) |
| [`TPB_PARM_LIST`](src/include/tpb-public.h:49) | `0x00020000` | 列表校验 (需传入 n, plist 有效值列表) |
| [`TPB_PARM_CUSTOM`](src/include/tpb-public.h:50) | `0x00040000` | 自定义校验函数 |

### 1.4 参数数据类型 (Type Flags, bits 0-15)

| 宏定义 | 值 | C 类型 | 说明 |
|--------|-----|-------|------|
| [`TPB_INT8_T`](src/include/tpb-public.h:55) | `0x00000137` | `int8_t` | 8 位有符号整数 |
| [`TPB_INT16_T`](src/include/tpb-public.h:56) | `0x00000238` | `int16_t` | 16 位有符号整数 |
| [`TPB_INT32_T`](src/include/tpb-public.h:57) | `0x00000439` | `int32_t` | 32 位有符号整数 |
| [`TPB_INT64_T`](src/include/tpb-public.h:58) | `0x0000083a` | `int64_t` | 64 位有符号整数 |
| [`TPB_UINT8_T`](src/include/tpb-public.h:59) | `0x0000013b` | `uint8_t` | 8 位无符号整数 |
| [`TPB_UINT16_T`](src/include/tpb-public.h:60) | `0x0000023c` | `uint16_t` | 16 位无符号整数 |
| [`TPB_UINT32_T`](src/include/tpb-public.h:61) | `0x0000043d` | `uint32_t` | 32 位无符号整数 |
| [`TPB_UINT64_T`](src/include/tpb-public.h:62) | `0x0000083e` | `uint64_t` | 64 位无符号整数 |
| [`TPB_FLOAT_T`](src/include/tpb-public.h:63) | `0x0000040a` | `float` | 单精度浮点 |
| [`TPB_DOUBLE_T`](src/include/tpb-public.h:64) | `0x0000080b` | `double` | 双精度浮点 |
| [`TPB_LONG_DOUBLE_T`](src/include/tpb-public.h:65) | `0x0000100c` | `long double` | 扩展精度浮点 |
| [`TPB_CHAR_T`](src/include/tpb-public.h:66) | `0x00000101` | `char` | 单字符 |
| [`TPB_STRING_T`](src/include/tpb-public.h:67) | `0x00001000` | `char*` | 字符串 |
| [`TPB_DTYPE_TIMER_T`](src/include/tpb-public.h:68) | `0x0000083F` | `tpb_timer_t` | 计时器类型 |

### 1.5 运行时参数结构 ([`tpb_rt_parm_t`](src/include/tpb-public.h:120))

```c
typedef struct tpb_rt_parm {
    char name[TPBM_NAME_STR_MAX_LEN];      // 参数名称 (最大 256 字符)
    char note[TPBM_NOTE_STR_MAX_LEN];      // 参数描述 (最大 2048 字符)
    tpb_parm_value_t value;                // 当前值
    TPB_DTYPE ctrlbits;                    // 控制位 (source|check|type)
    int nlims;                             // 限制值数量
    tpb_parm_value_t *plims;               // 限制值数组 (范围边界或列表值)
} tpb_rt_parm_t;
```

### 1.6 参数值联合体 ([`tpb_parm_value_t`](src/include/tpb-public.h:111))

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

## 2 输出参数控制属性 (Output Metric Attributes)

### 2.1 输出参数结构 ([`tpb_k_output_t`](src/include/tpb-public.h:131))

```c
typedef struct tpb_k_output {
    char name[TPBM_NAME_STR_MAX_LEN];  // 输出名称 (最大 256 字符)
    char note[TPBM_NOTE_STR_MAX_LEN];  // 输出描述 (最大 2048 字符)
    TPB_DTYPE dtype;                   // 数据类型
    TPB_UNIT_T unit;                   // 单位编码
    int n;                             // 元素数量 (运行时分配)
    void *p;                           // 数据指针 (运行时分配)
} tpb_k_output_t;
```

### 2.2 单位编码结构 (Unit Encoding, 64-bit)

```
+-------------+-------------+----------------+-----------+---------------------+
| 48-63       | 44-47       | 36-43          | 32-35     | 0-31                |
| Attribution | Unit Kind   | Unit Name Code | Base Type | Exponent/Multiplier |
+-------------+-------------+----------------+-----------+---------------------+
```

### 2.3 单位属性控制位 (Attribution Control Bits, 48-63)

| 宏定义 | 位 | 说明 |
|--------|-----|------|
| [`TPB_UATTR_CAST_MASK`](src/include/tpb-unitdefs.h:32) | bit 48 | 单位转换控制 |
| [`TPB_UATTR_CAST_Y`](src/include/tpb-unitdefs.h:33) | `1<<48` | 启用单位自动转换 |
| [`TPB_UATTR_CAST_N`](src/include/tpb-unitdefs.h:34) | `0<<48` | 禁用单位自动转换 |
| [`TPB_UATTR_SHAPE_MASK`](src/include/tpb-unitdefs.h:35) | bits 49-51 | 数据形状 (0-7 维数组) |
| [`TPB_UATTR_SHAPE_POINT`](src/include/tpb-unitdefs.h:36) | `0<<49` | 单点数据 |
| [`TPB_UATTR_SHAPE_1D`](src/include/tpb-unitdefs.h:37) | `1<<49` | 1 维数组 |
| [`TPB_UATTR_SHAPE_2D`](src/include/tpb-unitdefs.h:38) | `2<<49` | 2 维数组 |
| [`TPB_UATTR_SHAPE_3D`](src/include/tpb-unitdefs.h:39) | `3<<49` | 3 维数组 |
| [`TPB_UATTR_SHAPE_4D`](src/include/tpb-unitdefs.h:40) | `4<<49` | 4 维数组 |
| [`TPB_UATTR_SHAPE_5D`](src/include/tpb-unitdefs.h:41) | `5<<49` | 5 维数组 |
| [`TPB_UATTR_SHAPE_6D`](src/include/tpb-unitdefs.h:42) | `6<<49` | 6 维数组 |
| [`TPB_UATTR_SHAPE_7D`](src/include/tpb-unitdefs.h:43) | `7<<49` | 7 维数组 |
| [`TPB_UATTR_TRIM_MASK`](src/include/tpb-unitdefs.h:44) | bit 52 | 数值修约控制 |
| [`TPB_UATTR_TRIM_N`](src/include/tpb-unitdefs.h:45) | `1<<52` | 禁用修约 |
| [`TPB_UATTR_TRIM_Y`](src/include/tpb-unitdefs.h:46) | `0<<52` | 启用修约 (默认) |

### 2.4 单位种类 (Unit Kind, bits 44-47)

| 宏定义 | 值 | 说明 |
|--------|-----|------|
| [`TPB_UKIND_UNDEF`](src/include/tpb-unitdefs.h:50) | `0x0` | 未定义 |
| [`TPB_UKIND_TIME`](src/include/tpb-unitdefs.h:51) | `0x1` | 时间 |
| [`TPB_UKIND_VOL`](src/include/tpb-unitdefs.h:52) | `0x2` | 数量/容量 |
| [`TPB_UKIND_VOLPTIME`](src/include/tpb-unitdefs.h:53) | `0x3` | 速率 (数量/时间) |

### 2.5 单位名称代码 (Unit Name Code, bits 36-43)

#### 时间类 (TIME)

| 宏定义 | 说明 |
|--------|------|
| [`TPB_UNAME_WALLTIME`](src/include/tpb-unitdefs.h:56) | 墙钟时间 |
| [`TPB_UNAME_PHYSTIME`](src/include/tpb-unitdefs.h:57) | 芯片物理时间 |
| [`TPB_UNAME_DATETIME`](src/include/tpb-unitdefs.h:58) | 日期时间 |
| [`TPB_UNAME_TICKTIME`](src/include/tpb-unitdefs.h:59) | 滴答计数 |
| [`TPB_UNAME_TIMERTIME`](src/include/tpb-unitdefs.h:60) | 跟随计时器单位 |

#### 数量类 (VOL)

| 宏定义 | 说明 |
|--------|------|
| [`TPB_UNAME_DATASIZE`](src/include/tpb-unitdefs.h:61) | 数据大小 |
| [`TPB_UNAME_OP`](src/include/tpb-unitdefs.h:62) | 操作次数 |
| [`TPB_UNAME_GRIDSIZE`](src/include/tpb-unitdefs.h:63) | 网格数量 |
| [`TPB_UNAME_BITSIZE`](src/include/tpb-unitdefs.h:64) | 二进制位大小 |

#### 速率类 (VOL/TIME)

| 宏定义 | 说明 |
|--------|------|
| [`TPB_UNAME_OPS`](src/include/tpb-unitdefs.h:65) | 操作/秒 |
| [`TPB_UNAME_FLOPS`](src/include/tpb-unitdefs.h:66) | 浮点运算/秒 |
| [`TPB_UNAME_TOKENPS`](src/include/tpb-unitdefs.h:67) | Token/秒 |
| [`TPB_UNAME_TPS`](src/include/tpb-unitdefs.h:68) | 事务/秒 |
| [`TPB_UNAME_BITPS`](src/include/tpb-unitdefs.h:69) | bit/秒 |
| [`TPB_UNAME_DATAPS`](src/include/tpb-unitdefs.h:70) | Byte/秒 |
| [`TPB_UNAME_BITPCY`](src/include/tpb-unitdefs.h:71) | bit/周期 |
| [`TPB_UNAME_DATAPCY`](src/include/tpb-unitdefs.h:72) | Byte/周期 |
| [`TPB_UNAME_BITPTICK`](src/include/tpb-unitdefs.h:73) | bit/滴答 |
| [`TPB_UNAME_DATAPTICK`](src/include/tpb-unitdefs.h:74) | Byte/滴答 |

### 2.6 基数类型 (Base Type, bits 32-35)

| 宏定义 | 说明 |
|--------|------|
| [`TPB_UBASE_BASE`](src/include/tpb-unitdefs.h:80) | 基础单位 |
| [`TPB_UBASE_BIN_EXP_P`](src/include/tpb-unitdefs.h:81) | 二进制指数 (正) |
| [`TPB_UBASE_BIN_EXP_N`](src/include/tpb-unitdefs.h:82) | 二进制指数 (负) |
| [`TPB_UBASE_BIN_MUL_P`](src/include/tpb-unitdefs.h:83) | 二进制乘数 (正) |
| [`TPB_UBASE_OCT_EXP_P`](src/include/tpb-unitdefs.h:84) | 八进制指数 (正) |
| [`TPB_UBASE_OCT_EXP_N`](src/include/tpb-unitdefs.h:85) | 八进制指数 (负) |
| [`TPB_UBASE_DEC_EXP_P`](src/include/tpb-unitdefs.h:86) | 十进制指数 (正) |
| [`TPB_UBASE_DEC_EXP_N`](src/include/tpb-unitdefs.h:87) | 十进制指数 (负) |
| [`TPB_UBASE_DEC_MUL_P`](src/include/tpb-unitdefs.h:88) | 十进制乘数 (正) |
| [`TPB_UBASE_HEX_EXP_P`](src/include/tpb-unitdefs.h:89) | 十六进制指数 (正) |
| [`TPB_UBASE_HEX_EXP_N`](src/include/tpb-unitdefs.h:90) | 十六进制指数 (负) |

### 2.7 常用单位宏 (Common Unit Macros)

#### 时间单位

| 宏定义 | 说明 |
|--------|------|
| [`TPB_UNIT_NS`](src/include/tpb-unitdefs.h:126) | 纳秒 |
| [`TPB_UNIT_US`](src/include/tpb-unitdefs.h:130) | 微秒 |
| [`TPB_UNIT_MS`](src/include/tpb-unitdefs.h:131) | 毫秒 |
| [`TPB_UNIT_SS`](src/include/tpb-unitdefs.h:132) | 秒 |
| [`TPB_UNIT_CY`](src/include/tpb-unitdefs.h:134) | 周期 |

#### 数据量单位

| 宏定义 | 说明 |
|--------|------|
| [`TPB_UNIT_BYTE`](src/include/tpb-unitdefs.h:106) | 字节 |
| [`TPB_UNIT_KIB`](src/include/tpb-unitdefs.h:107) | KiB (2^10) |
| [`TPB_UNIT_MIB`](src/include/tpb-unitdefs.h:108) | MiB (2^20) |
| [`TPB_UNIT_GIB`](src/include/tpb-unitdefs.h:109) | GiB (2^30) |
| [`TPB_UNIT_KB`](src/include/tpb-unitdefs.h:117) | KB (10^3) |
| [`TPB_UNIT_MB`](src/include/tpb-unitdefs.h:118) | MB (10^6) |
| [`TPB_UNIT_GB`](src/include/tpb-unitdefs.h:119) | GB (10^9) |

#### 性能单位

| 宏定义 | 说明 |
|--------|------|
| [`TPB_UNIT_GFLOPS`](src/include/tpb-unitdefs.h:176) | GFLOPS (10^9) |
| [`TPB_UNIT_TFLOPS`](src/include/tpb-unitdefs.h:177) | TFLOPS (10^12) |
| [`TPB_UNIT_GBPS`](src/include/tpb-unitdefs.h:217) | GB/s |

---

## 3 使用示例

### 3.1 添加输入参数

```c
// 从命令行传入的 int64 参数，范围校验 [1, 100000]
tpb_k_add_parm("ntest", "Number of test iterations", "10",
               TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_RANGE,
               1, 100000);

// 从命令行传入的 double 参数，范围校验 [0, 10000]
tpb_k_add_parm("twarm", "Warm-up time in ms", "100",
               TPB_PARM_CLI | TPB_DOUBLE_T | TPB_PARM_RANGE,
               0, 10000);
```

### 3.2 添加输出指标

```c
// 添加时间输出，单位为纳秒
tpb_k_add_output("elapsed_time", "Elapsed wall-clock time",
                 TPB_INT64_T, TPB_UNIT_NS);

// 添加性能输出，单位为 GFLOPS
tpb_k_add_output("performance", "Peak performance",
                 TPB_DOUBLE_T, TPB_UNIT_GFLOPS);

// 添加数据量输出，单位为 GiB
tpb_k_add_output("data_size", "Total data processed",
                 TPB_UINT64_T, TPB_UNIT_GIB);
```

---

## 4 属性总结表

| 属性类别 | 输入参数 | 输出参数 |
|----------|----------|----------|
| **名称** | `name` (256 字符) | `name` (256 字符) |
| **描述** | `note` (2048 字符) | `note` (2048 字符) |
| **值/数据** | `value` | `p` (指针) + `n` (数量) |
| **类型** | `ctrlbits` (32 位编码) | `dtype` (数据类型) |
| **校验** | `nlims` + `plims` | 无 |
| **单位** | 无 | `unit` (64 位编码) |
| **来源** | Source flags (CLI/Macro/File/Env) | N/A |
| **形状** | N/A | Shape flags (Point/1D-7D) |
| **转换** | N/A | Cast/Trim flags |