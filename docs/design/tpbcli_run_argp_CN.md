# `tpbcli run` 参数解析工作流程

本文档分析 `tpbcli run` 子命令的参数解析工作流程，涵盖从命令行输入到最终生成多个执行 handle 的完整链路。

## 1. 整体架构

```
tpbcli_run()
  ├── tpb_register_kernel()          // 注册 _tpb_common 元数据 + 扫描 .so（不创建 handle）
  ├── parse_run()                    // 解析命令行参数
  │   ├── --kernel                   // 创建新 handle（首个为 handle_list[0]）
  │   ├── --kargs …                  // 须在 --kernel 之后；设置当前 handle
  │   ├── --kargs-dim …              // 须在 --kernel 之后；延迟展开
  │   ├── --kenvs / --kenvs-dim      // 须在 --kernel 之后
  │   ├── --kmpiargs / --kmpiargs-dim // 须在 --kernel 之后
  │   ├── --timer                    // 定时器
  │   └── --outargs                  // 输出格式
  └── tpb_driver_run_all()           // 执行 handle_list[0..nhdl-1]
```

**源文件对应关系：**

| 文件 | 职责 |
|------|------|
| `src/tpbcli-run.c` | `run` 子命令入口、参数分发、dim展开 |
| `src/tpbcli-run-dim.c` | 维度语法解析（列表、递归序列、嵌套） |
| `src/tpbcli-run-dim.h` | 维度类型定义、数据结构 |
| `src/corelib/tpb-driver.c` | Handle管理、参数设置、内核执行 |
| `src/corelib/tpb-argp.c` | 通用参数解析工具（token分割、类型转换、范围校验） |

## 2. 入口：`tpbcli_run()`

位置：`src/tpbcli-run.c` 中 `tpbcli_run()`

```c
int tpbcli_run(int argc, char **argv)
{
    // 1. 注册内核元数据、扫描 .so；handle_list 为空直至首个 --kernel
    tpb_register_kernel();

    // 2. 解析命令行参数（核心）
    parse_run(argc, argv);

    // 3. 执行所有 handle
    tpb_driver_run_all();
}
```

### 2.1 `tpb_register_kernel()` 初始化

位置：`src/corelib/tpb-driver.c`

```
tpb_register_kernel()
  ├── tpb_register_common()          // 填充 kernel_common（_tpb_common），供 tpb_query_kernel 等使用
  ├── tpb_dl_scan()                  // 动态扫描 lib/ 下的 .so 内核库
  └── handle_list = NULL, nhdl = 0, current_rthdl = NULL
```

`_tpb_common` 仍描述一组“文档/合并用”的公共参数名，但**不再**对应运行时的伪 handle。CLI 在第一个 `--kernel` 之前不允许使用任何 `--kargs*` / `--kenvs*` / `--kmpiargs*`。

公共参数（`kernel_common`）默认值（仅元数据，与具体内核是否实现同名参数无关）：

| 参数 | 类型 | 默认值 | 范围 |
|------|------|--------|------|
| `ntest` | int64 | 10 | [1, 100000] |
| `twarm` | int64 | 100 | [0, 10000] |
| `total_memsize` | double | 32 (KiB) | [0.0009765625, DBL_MAX] |

## 3. 核心解析：`parse_run()`

位置：`src/tpbcli-run.c:127`

从 `argv[2]` 开始遍历（`argv[0]` 是程序名，`argv[1]` 是 `run`），维护两个关键状态变量：

```c
tpb_dim_config_t *pending_dim_cfg = NULL;        // 待展开的dim配置链表
char pending_kernel_name[TPBM_NAME_STR_MAX_LEN]; // 当前内核名
```

### 3.1 各选项处理逻辑

| 选项 | 行为 | 是否立即生效 |
|------|------|-------------|
| `--kernel/-k <name>` | 触发前一个kernel的dim展开 → `tpb_driver_add_handle(name)` 创建新handle → 更新 `pending_kernel_name` | 立即 |
| `--kargs k=v,k=v` | 须已有 `pending_kernel_name`；按逗号分割 → `tpb_driver_set_hdl_karg()` | 立即 |
| `--kargs-dim 'parm=spec'` | 须已有 `pending_kernel_name`；解析后链入 `pending_dim_cfg`，**延迟展开** | **延迟** |
| `--kenvs K=V,K=V` | 须已有 `pending_kernel_name`；`parse_kenvs_tokstr()` | 立即 |
| `--kenvs-dim '...'` | 须已有 `pending_kernel_name`；解析后**立即** `expand_env_dim_handles()` | 立即 |
| `--kmpiargs '...'` | 须已有 `pending_kernel_name`；`parse_kmpiargs_quoted()` → **append**（多次拼接） | 立即 |
| `--kmpiargs-dim '[...]{...}'` | 须已有 `pending_kernel_name`；`expand_kmpiargs_dim()` **立即展开** | 立即 |
| `--timer <name>` | 设置定时器类型（clock_gettime / tsc_asym） | 立即 |
| `--outargs ...` | 设置输出格式（unit_cast, sigbit_trim） | 立即 |

**关键设计**：`--kargs-dim` 是**延迟展开**的——它只收集配置，直到遇到下一个 `--kernel` 或解析结束时才触发 `expand_dim_handles()`。这使得 `--kargs` 设置的参数可以作为所有dim组合的公共基础。

### 3.2 多段 `--kargs-dim` 的链式连接

在 `parse_run()` 中，多次出现 `--kargs-dim` 会通过 `nested` 指针形成链表：

```bash
tpbcli run --kernel stream \
  --kargs-dim 'A=[1,2]' \
  --kargs-dim 'B=[x,y]'
```

```
第一次:  pending_dim_cfg → cfg_A
第二次:  找到tail (cfg_A), tail->nested = cfg_B
结果:    cfg_A → nested → cfg_B
```

### 3.3 `--kernel` 切换时的dim展开

当遇到新的 `--kernel` 时，会先展开前一个kernel的pending dim配置：

```c
if (strcmp(argv[i], "--kernel") == 0) {
    // 展开前一个kernel的pending dim
    if (pending_dim_cfg != NULL && pending_kernel_name[0] != '\0') {
        expand_dim_handles(pending_dim_cfg, pending_kernel_name);
        tpb_dim_config_free(pending_dim_cfg);
        pending_dim_cfg = NULL;
    }
    // 创建新kernel的handle
    tpb_driver_add_handle(argv[i+1]);
    pending_kernel_name = argv[i+1];
}
```

解析结束后，还会检查并展开剩余的pending配置（`parse_run()` 末尾）。

## 4. `--kargs-dim` 参数解析：`tpb_argp_parse_dim()`

位置：`src/tpbcli-run-dim.c:457`

输入格式为 `parm_name=spec`，自动识别三种语法：

### 4.1 语法识别路由

```
tpb_argp_parse_dim("total_memsize=[32768,524288,3145728]")
  │
  ├─ 检查是否有 '{' 在 '=' 之前 → 嵌套模式 → tpb_argp_parse_dim_nest()
  │
  ├─ 分离 parm_name 和 spec
  │
  └─ 根据 spec 首字符路由：
       ├─ '(' → 已废弃的线性序列 (st,en,step) → 报错并提示使用递归格式
       ├─ '[' → 显式列表 → tpb_argp_parse_list()
       └─ 字母开头 → 递归序列 → tpb_argp_parse_dim_recur()
```

### 4.2 三种维度类型

#### 类型A：显式列表 `[a, b, c, ...]`

```c
// tpb_argp_parse_list()
// 输入: "[32768, 524288, 3145728]"
// 输出: tpb_dim_config_t {
//   type = TPB_DIM_LIST,
//   spec.list = { n=3, values=[32768.0, 524288.0, 3145728.0], is_string=0 }
// }
```

支持混合字符串和数值。如果任一元素不是纯数字，`is_string` 标记为 1，后续使用字符串值。

示例：
```bash
--kargs-dim 'dtype=[double,float,iso-fp16]'
```

#### 类型B：递归序列 `op(@,x)(st,min,max,nlim)`

```c
// tpb_argp_parse_dim_recur()
// 输入: "mul(@,2)(16,16,128,0)"
// 解析:
//   op = TPB_DIM_OP_MUL, x = 2
//   st = 16, min = 16, max = 128, nlim = 0
// 生成值: 16 → 32 → 64 → 128 (每次*2, 在[min,max]内, nlim=0表示无步数限制)
```

支持的运算符：

| 运算符 | 含义 | 示例 | 生成序列 |
|--------|------|------|---------|
| `add` | `@ + x` | `add(@,128)(128,128,512,4)` | 128, 256, 384, 512 |
| `sub` | `@ - x` | `sub(@,10)(100,10,50,0)` | 100, 90, 80, 70, 60, 50 |
| `mul` | `@ * x` | `mul(@,2)(16,16,128,0)` | 16, 32, 64, 128 |
| `div` | `@ / x` | `div(@,2)(128,16,128,0)` | 128, 64, 32, 16 |
| `pow` | `@ ^ x` | `pow(@,2)(2,2,256,0)` | 2, 4, 16, 256 |

校验规则：
- `min <= max`
- `st` 必须在 `[min, max]` 范围内
- 值超出 `[min, max]` 范围时停止生成
- 达到 `nlim` 步数时停止（`nlim=0` 表示仅受范围限制）

#### 类型C：嵌套 `outer{inner}`

```c
// tpb_argp_parse_dim_nest()
// 输入: "dtype=[double,float]{total_memsize=mul(@,2)(16,16,128,0)}"
// 输出: cfg(dtype) → nested → cfg(total_memsize)
// 支持多层嵌套: A{B{C{...}}}
```

嵌套通过 `tpb_dim_config_t` 的 `nested` 指针形成链表，最大深度 `TPBM_DIM_MAX_NEST_DEPTH = 8`。

### 4.3 数据结构

```c
// 配置结构（解析后）
typedef struct tpb_dim_config {
    char parm_name[TPBM_NAME_STR_MAX_LEN];  // 参数名
    tpb_dim_type_t type;                     // TPB_DIM_LIST 或 TPB_DIM_RECUR
    union {
        struct { int n; char **str_values; double *values; int is_string; } list;
        struct { tpb_dim_op_t op; double x, st, min, max; int nlim; } recur;
    } spec;
    struct tpb_dim_config *nested;           // 嵌套维度
} tpb_dim_config_t;

// 值结构（生成后）
typedef struct tpb_dim_values {
    char parm_name[TPBM_NAME_STR_MAX_LEN];
    int n;
    char **str_values;
    double *values;
    int is_string;
    struct tpb_dim_values *nested;
} tpb_dim_values_t;
```

## 5. 值生成：`tpb_dim_generate_values()`

位置：`src/tpbcli-run-dim.c:555`

将 `tpb_dim_config_t`（解析后的配置）转换为 `tpb_dim_values_t`（具体值数组）：

```c
tpb_dim_generate_values(cfg, &dim_vals)
  │
  ├─ TPB_DIM_LIST:
  │   直接复制 str_values 和 values 数组
  │
  ├─ TPB_DIM_RECUR:
  │   current = st
  │   while (current in [min,max] && n < nlim):
  │     values[n++] = current
  │     current = op(current, x)  // add/sub/mul/div/pow
  │
  └─ 如果有 nested:
      递归调用 tpb_dim_generate_values(cfg->nested, &val->nested)
```

递归序列的生成循环（`nlim=0` 时上限为 `TPBM_DIM_MAX_VALUES = 4096`）：

```c
while (n < cap) {
    if (current < min || current > max) break;
    val->values[n] = current;
    n++;
    if (nlim > 0 && n >= nlim) break;
    // 计算下一个值
    switch (op) {
        case TPB_DIM_OP_ADD: current = current + x; break;
        case TPB_DIM_OP_SUB: current = current - x; break;
        case TPB_DIM_OP_MUL: current = current * x; break;
        case TPB_DIM_OP_DIV: if (x == 0) break; current = current / x; break;
        case TPB_DIM_OP_POW: current = pow(current, x); break;
    }
}
```

## 6. Handle展开：`expand_dim_handles()`

位置：`src/tpbcli-run.c:486`

这是**最终参数生成**的关键步骤——将维度配置展开为多个handle（每个组合一个handle）：

```c
expand_dim_handles(dim_cfg, kernel_name)
  ├── tpb_dim_generate_values(dim_cfg, &dim_vals)   // 生成值数组
  ├── tpb_dim_get_total_count(dim_cfg)               // 计算总组合数
  ├── expand_nested_dims(dim_vals, indices, ...)     // 递归展开笛卡尔积
  └── 清理 dim_vals 和 indices
```

### 6.1 `expand_nested_dims_impl()` 的笛卡尔积展开

```c
// 以 --kargs-dim 'A=[1,2]{B=[x,y]}' 为例:
// dim_vals: A=[1,2] → nested → B=[x,y]
// 深度=2, 总组合=2*2=4

expand_nested_dims_impl(vals, indices, depth=0, max_depth=2, kernel_name, &is_first)
  │
  ├─ depth=0 (外层A): 遍历 i=0,1
  │   └─ depth=1 (内层B): 遍历 j=0,1
  │       ├─ is_first=1: 修改现有handle (ihdl=1), 设置 A=1, B=x
  │       ├─ is_first=0: add_handle("stream"), 设置 A=1, B=y
  │       ├─ is_first=0: add_handle("stream"), 设置 A=2, B=x
  │       └─ is_first=0: add_handle("stream"), 设置 A=2, B=y
```

**关键行为**：
- 第一个组合**复用**已存在的handle（由 `--kernel` 创建的那个）
- 后续组合调用 `tpb_driver_add_handle()` 创建新handle
- 每个组合按**从外到内**的顺序应用所有维度值

### 6.2 `apply_dim_value()` — 将维度值设置到handle

位置：`src/tpbcli-run.c:350`

```c
apply_dim_value(dim_val, index)
  ├── 如果是字符串: tpb_driver_set_hdl_karg(parm_name, str_values[index])
  └── 如果是数值:
        ├── 整数: 格式化为 "%lld"
        └── 浮点: 格式化为 "%.15g"
        → tpb_driver_set_hdl_karg(parm_name, value_str)
```

`tpb_driver_set_hdl_karg()` 会：
1. 在当前handle的 `argpack` 中查找参数名
2. 如果未找到且是 `_tpb_common`，从 `kernel_common.info.parms` 中添加
3. 根据参数的 `ctrlbits` 类型码（int/uint/float/double/char）解析字符串值
4. 执行范围校验（`TPB_PARM_RANGE`）或列表校验（`TPB_PARM_LIST`）
5. 更新 `parm->value`

## 7. `--kenvs-dim` 和 `--kmpiargs-dim` 的差异化处理

| 维度类型 | 展开时机 | 展开方式 |
|---------|---------|---------|
| `--kargs-dim` | **延迟**（遇到下一个 `--kernel` 或解析结束时） | 递归笛卡尔积，第一个组合修改现有handle |
| `--kenvs-dim` | **立即** | 递归笛卡尔积，后续组合调用 `tpb_driver_copy_hdl_from()` 复制kargs |
| `--kmpiargs-dim` | **立即** | 解析 `['opt1','opt2']{['a','b']}` 格式，后续组合复制handle |

### 7.1 `--kenvs-dim` 的特殊性

`expand_env_dim_handles()` 在展开时，后续组合会：
1. `tpb_driver_add_handle()` 创建新handle
2. `tpb_driver_copy_hdl_from(orig_hdl_idx)` 从原始handle复制kargs和envs
3. 再应用当前组合的环境变量维度值

这确保每个env维度组合都携带相同的kargs基础配置。

### 7.2 `--kmpiargs-dim` 的显式列表语法

```bash
--kmpiargs-dim "['-np 2','-np 4']{['--bind-to core','--bind-to none']}"
```

解析流程：
1. `parse_kmpiargs_list()` 解析外层 `['-np 2','-np 4']`
2. 检查是否有 `{` 开头的嵌套列表
3. 计算总组合数 = outer_count × inner_count
4. 对每个组合：
   - 第一个：修改现有handle
   - 后续：`add_handle()` + `copy_hdl_from()` + 重置mpiargs + 追加组合值

## 8. Handle 创建与参数继承

### 8.1 `tpb_driver_add_handle()` 的参数初始化

位置：`src/corelib/tpb-driver.c:979`

```c
tpb_driver_add_handle(kernel_name)
  ├── 查找内核元数据
  ├── 分配新 handle (handle_list[nhdl]，首个 handle 下标为 0)
  ├── 复制 kernel info
  ├── 构建 argpack: 从 kernel->info.parms 复制默认值（无伪 handle 覆盖）
  ├── envpack 置空
  └── mpipack 置空
```

### 8.2 `tpb_driver_copy_hdl_from()` — 复制已有配置

位置：`src/corelib/tpb-driver.c:1374`

用于 `--kenvs-dim` 和 `--kmpiargs-dim` 展开时创建新handle：

```c
tpb_driver_copy_hdl_from(src_idx)
  ├── 复制 argpack 中同名参数的 value
  ├── 复制 envpack（完整替换）
  └── 复制 mpipack（完整替换）
```

注意：`--kargs-dim` 展开时**不使用** `copy_hdl_from`：第一个组合改当前 handle，后续组合 `add_handle` 后仅带 **内核注册默认值**，再应用各维度的值（不会自动复制上一组合在 `--kargs` 里改过的参数）。

## 9. 完整流程示例

```bash
tpbcli run --kernel stream --kargs ntest=20 \
  --kargs-dim 'stream_array_size=[32768,524288,3145728]'
```

（`stream` 内核使用 `stream_array_size`；其它内核按其注册的参数名书写。）

### 执行序列：

```
1. tpb_register_kernel()
   → nhdl=0, handle_list=NULL

2. parse_run() 遍历参数:

   a. --kernel stream
      → tpb_driver_add_handle("stream")
         → handle_list[0]: stream 内核默认值
         → nhdl=1, ihdl=0
      → pending_kernel_name = "stream"

   b. --kargs ntest=20
      → handle_list[0].argpack 中 ntest = 20

   c. --kargs-dim 'stream_array_size=[32768,524288,3145728]'
      → pending_dim_cfg = cfg (延迟展开)

3. parse_run() 结束, 触发剩余 dim 展开:
   → expand_dim_handles(...)
      → 组合0: 改 handle_list[0], stream_array_size=32768
      → 组合1: add_handle → handle_list[1], 仅内核默认 + dim 值 524288
      → 组合2: add_handle → handle_list[2], 仅内核默认 + dim 值 3145728
      → nhdl=3

4. tpb_driver_run_all()
   → 遍历 handle_list[0..2]
```

### 最终 handle 状态（示意）

| Handle | 内核 | ntest | stream_array_size | 说明 |
|--------|------|-------|-------------------|------|
| [0] | stream | **20** | **32768** | `--kargs` + dim 首组 |
| [1] | stream | 10（内核默认） | **524288** | 新 handle，不继承 [0] 上 `--kargs` |
| [2] | stream | 10（内核默认） | **3145728** | 同上 |

若希望每个 dim 组合都带 `ntest=20`，请使用嵌套 `--kargs-dim`（例如 `ntest=[20]{stream_array_size=[...]}`）或在每个组合前用脚本/多次 `--kernel` 显式设置。

## 10. 关键数据结构关系

```
handle_list[0]  → 第一个 --kernel 创建的句柄（可被 dim 首组就地修改）
handle_list[1]  → 第二个组合或第二个 --kernel …
...

每个handle (tpb_k_rthdl_t) 包含:
  ├── kernel:     内核元数据(info.name, info.parms默认值, info.outs)
  ├── argpack:    运行时参数数组 (从kernel默认值初始化, 被--kargs/--kargs-dim覆盖)
  │   └── args[]: tpb_rt_parm_t { name, value, ctrlbits, plims, nlims }
  ├── envpack:    环境变量 (被--kenvs/--kenvs-dim设置)
  │   └── envs[]: tpb_env_entry_t { name, value }
  ├── mpipack:    MPI参数 (被--kmpiargs/--kmpiargs-dim设置)
  │   └── mpiargs: char* (如 "-np 2 --bind-to core")
  └── respack:    运行时输出 (内核执行时填充)
      └── outputs[]: tpb_k_output_t { name, dtype, unit, n, p }
```

## 11. 参数优先级

对**同一个** handle、**同一**参数名，大致优先级（高到低）：

1. **`--kargs-dim` 展开** — `apply_dim_value()` 写入的值  
2. **同段内较后的 `--kargs`** — 覆盖较早的 `--kargs`  
3. **内核注册默认值** — `tpb_k_add_parm()` 的 default_val  

`add_handle` 不再从伪 handle 合并 `_tpb_common`。dim 展开产生的新 handle 从内核默认值开始，再套 dim；不自动复制上一 handle 上仅用 `--kargs` 改过的字段。
