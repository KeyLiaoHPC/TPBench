# `tpbcli run` Argument Parsing Workflow

This document analyzes the argument parsing workflow of the `tpbcli run` subcommand, covering the complete pipeline from CLI input to the generation of multiple execution handles.

## 1. Overall Architecture

```
tpbcli_run()
  ├── tpb_register_kernel()          // Scan PLI kernel .so libraries (no handle created yet)
  ├── parse_run()                    // Parse CLI arguments
  │   ├── --kernel                   // Create new handle (first becomes handle_list[0])
  │   ├── --kargs …                  // Must follow --kernel; sets current handle
  │   ├── --kargs-dim …              // Must follow --kernel; deferred expansion
  │   ├── --kenvs / --kenvs-dim      // Must follow --kernel
  │   ├── --kmpiargs / --kmpiargs-dim // Must follow --kernel
  │   ├── --timer                    // Timer backend
  │   └── --outargs                  // Output formatting
  └── tpb_driver_run_all()           // Execute handle_list[0..nhdl-1]
```

**Source file mapping:**

| File | Responsibility |
|------|----------------|
| `src/tpbcli-run.c` | `run` subcommand entry, argument dispatch, dim expansion |
| `src/tpbcli-run-dim.c` | Dimension syntax parsing (list, recursive sequence) |
| `src/tpbcli-run-dim.h` | Dimension type definitions, data structures |
| `src/corelib/tpb-driver.c` | Handle management, parameter setting, kernel execution |
| `src/corelib/tpb-argp.c` | `--kargs` comma-split helper (`tpb_argp_set_kargs_tokstr`); timer name dispatch |

## 2. Entry Point: `tpbcli_run()`

Location: `tpbcli_run()` in `src/tpbcli-run.c`

```c
int tpbcli_run(int argc, char **argv)
{
    // 1. Register kernel metadata, scan .so; handle_list remains NULL until first --kernel
    tpb_register_kernel();

    // 2. Parse CLI arguments (core logic)
    parse_run(argc, argv);

    // 3. Execute all handles
    tpb_driver_run_all();
}
```

### 2.1 `tpb_register_kernel()` Initialization

Location: `src/corelib/tpb-driver.c`

```
tpb_register_kernel()
  ├── tpb_dl_scan()                  // Dynamically scan .so kernel libraries under lib/
  └── handle_list = NULL, nhdl = 0, current_rthdl = NULL
```

There is no synthetic `_tpb_common` kernel or shared `kernel_common` metadata block: each real kernel supplies its own parameter definitions via PLI registration. The CLI disallows any `--kargs*` / `--kenvs*` / `--kmpiargs*` before the first `--kernel`.

## 3. Core Parsing: `parse_run()`

Location: `src/tpbcli-run.c:127`

Iterates from `argv[2]` (`argv[0]` is the program name, `argv[1]` is `run`), maintaining two key state variables:

```c
tpb_dim_config_t *pending_dim_cfg = NULL;        // Linked list of pending dim configs
char pending_kernel_name[TPBM_NAME_STR_MAX_LEN]; // Current kernel name
```

### 3.1 Per-Option Handling Logic

| Option | Behavior | Immediate? |
|--------|----------|------------|
| `--kernel/-k <name>` | Traces previous kernel's dim expansion → `tpb_driver_add_handle(name)` creates new handle → updates `pending_kernel_name` | Immediate |
| `--kargs k=v,k=v` | Requires `pending_kernel_name`; comma-split → `tpb_driver_set_hdl_karg()` | Immediate |
| `--kargs-dim 'parm=spec'` | Requires `pending_kernel_name`; parsed and chained into `pending_dim_cfg`, **deferred expansion** | **Deferred** |
| `--kenvs K=V,K=V` | Requires `pending_kernel_name`; `parse_kenvs_tokstr()` | Immediate |
| `--kenvs-dim '...'` | Requires `pending_kernel_name`; parsed then **immediately** `expand_env_dim_handles()` | Immediate |
| `--kmpiargs '...'` | Requires `pending_kernel_name`; `parse_kmpiargs_quoted()` → **append** (multiple calls concatenate) | Immediate |
| `--kmpiargs-dim '[...]'` | Requires `pending_kernel_name`; `expand_kmpiargs_dim()` **immediately expands** | Immediate |
| `--timer <name>` | Set timer backend (clock_gettime / tsc_asym) | Immediate |
| `--outargs ...` | Set output formatting (unit_cast, sigbit_trim) | Immediate |

**Key design**: `--kargs-dim` uses **deferred expansion** — it only collects configurations until the next `--kernel` or the end of parsing triggers `expand_dim_handles()`. This allows `--kargs`-set parameters to serve as a common base across all dim combinations.

### 3.2 Chaining Multiple `--kargs-dim` Segments

In `parse_run()`, multiple `--kargs-dim` occurrences are chained into a linked list. Each new `--kargs-dim` is appended to the tail of the existing list:

```bash
tpbcli run --kernel stream \
  --kargs-dim 'A=[1,2]' \
  --kargs-dim 'B=[x,y]'
```

```
First:   pending_dim_cfg → cfg_A
Second:  Find tail (cfg_A), append cfg_B
Result:  cfg_A → next → cfg_B
```

The linked list structure allows building a flat list of dimension configurations, which are then expanded as a Cartesian product during handle expansion.

### 3.3 Dim Expansion on `--kernel` Switch

When a new `--kernel` is encountered, the pending dim config of the previous kernel is expanded first:

```c
if (strcmp(argv[i], "--kernel") == 0) {
    // Expand previous kernel's pending dim
    if (pending_dim_cfg != NULL && pending_kernel_name[0] != '\0') {
        expand_dim_handles(pending_dim_cfg, pending_kernel_name);
        tpb_dim_config_free(pending_dim_cfg);
        pending_dim_cfg = NULL;
    }
    // Create new kernel's handle
    tpb_driver_add_handle(argv[i+1]);
    pending_kernel_name = argv[i+1];
}
```

After parsing completes, any remaining pending config is checked and expanded (at the end of `parse_run()`).

## 4. `--kargs-dim` Parsing: `tpb_argp_parse_dim()`

Location: `src/tpbcli-run-dim.c:374`

Input format is `parm_name=spec`, with automatic detection of two syntaxes:

### 4.1 Syntax Routing

```
tpb_argp_parse_dim("total_memsize=[32768,524288,3145728]")
  │
  ├─ Check if '{' appears → Error: nested syntax not supported
  │
  ├─ Split parm_name and spec
  │
  └─ Route based on spec's first character:
       ├─ '(' → Deprecated linear sequence (st,en,step) → error with hint to use recursive format
       ├─ '[' → Explicit list → tpb_argp_parse_list()
       └─ Letter → Recursive sequence → tpb_argp_parse_dim_recur()
```

### 4.2 Two Dimension Types

#### Type A: Explicit List `[a, b, c, ...]`

```c
// tpb_argp_parse_list()
// Input: "[32768, 524288, 3145728]"
// Output: tpb_dim_config_t {
//   type = TPB_DIM_LIST,
//   spec.list = { n=3, values=[32768.0, 524288.0, 3145728.0], is_string=0 }
// }
```

Supports mixed strings and numbers. If any element is non-numeric, `is_string` is set to 1, and string values are used downstream.

Example:
```bash
--kargs-dim 'dtype=[double,float,iso-fp16]'
```

#### Type B: Recursive Sequence `op(@,x)(st,min,max,nlim)`

```c
// tpb_argp_parse_dim_recur()
// Input: "mul(@,2)(16,16,128,0)"
// Parsed:
//   op = TPB_DIM_OP_MUL, x = 2
//   st = 16, min = 16, max = 128, nlim = 0
// Generated: 16 → 32 → 64 → 128 (×2 each step, within [min,max], nlim=0 means no step limit)
```

Supported operators:

| Operator | Meaning | Example | Generated Sequence |
|----------|---------|---------|--------------------|
| `add` | `@ + x` | `add(@,128)(128,128,512,4)` | 128, 256, 384, 512 |
| `sub` | `@ - x` | `sub(@,10)(100,10,50,0)` | 100, 90, 80, 70, 60, 50 |
| `mul` | `@ * x` | `mul(@,2)(16,16,128,0)` | 16, 32, 64, 128 |
| `div` | `@ / x` | `div(@,2)(128,16,128,0)` | 128, 64, 32, 16 |
| `pow` | `@ ^ x` | `pow(@,2)(2,2,256,0)` | 2, 4, 16, 256 |

Validation rules:
- `min <= max`
- `st` must be within `[min, max]`
- Generation stops when value exceeds `[min, max]`
- Generation stops when `nlim` steps reached (`nlim=0` means range-bound only)

### 4.3 Data Structures

```c
// Config structure (after parsing)
typedef struct tpb_dim_config {
    char parm_name[TPBM_NAME_STR_MAX_LEN];  // Parameter name
    tpb_dim_type_t type;                     // TPB_DIM_LIST or TPB_DIM_RECUR
    union {
        struct { int n; char **str_values; double *values; int is_string; } list;
        struct { tpb_dim_op_t op; double x, st, min, max; int nlim; } recur;
    } spec;
} tpb_dim_config_t;

// Values structure (after generation)
typedef struct tpb_dim_values {
    char parm_name[TPBM_NAME_STR_MAX_LEN];
    int n;
    char **str_values;
    double *values;
    int is_string;
} tpb_dim_values_t;
```

Note: The `nested` pointer has been removed. Multiple dimensions are now handled via a linked list at the CLI layer (chained via `next` pointer in `pending_dim_cfg`), not within individual dimension configs.

## 5. Value Generation: `tpb_dim_generate_values()`

Location: `src/tpbcli-run-dim.c:457`

Converts `tpb_dim_config_t` (parsed config) to `tpb_dim_values_t` (concrete value array):

```c
tpb_dim_generate_values(cfg, &dim_vals)
  │
  ├─ TPB_DIM_LIST:
  │   Directly copy str_values and values arrays
  │
  └─ TPB_DIM_RECUR:
      current = st
      while (current in [min,max] && n < nlim):
        values[n++] = current
        current = op(current, x)  // add/sub/mul/div/pow
```

Recursive sequence generation loop (capped at `TPBM_DIM_MAX_VALUES = 4096` when `nlim=0`):

```c
while (n < cap) {
    if (current < min || current > max) break;
    val->values[n] = current;
    n++;
    if (nlim > 0 && n >= nlim) break;
    // Compute next value
    switch (op) {
        case TPB_DIM_OP_ADD: current = current + x; break;
        case TPB_DIM_OP_SUB: current = current - x; break;
        case TPB_DIM_OP_MUL: current = current * x; break;
        case TPB_DIM_OP_DIV: if (x == 0) break; current = current / x; break;
        case TPB_DIM_OP_POW: current = pow(current, x); break;
    }
}
```

## 6. Handle Expansion: `expand_dim_handles()`

Location: `src/tpbcli-run.c:486`

This is the **final parameter generation** step — expanding dimension configs into multiple handles (one per combination):

```c
expand_dim_handles(dim_cfg, kernel_name)
  ├── tpb_dim_generate_values(dim_cfg, &dim_vals)   // Generate value arrays
  ├── tpb_dim_get_total_count(dim_cfg)               // Compute total combination count
  ├── expand_dim_handles_impl(dim_vals, indices, ...) // Expand Cartesian product
  └── Clean up dim_vals and indices
```

### 6.1 Cartesian Product Expansion in `expand_dim_handles_impl()`

```c
// Example: --kargs-dim 'A=[1,2]' --kargs-dim 'B=[x,y]'
// dim_cfg: A=[1,2] → next → B=[x,y]
// Total dimensions=2, total combinations=2×2=4

expand_dim_handles_impl(dim_list, indices, depth=0, total_dims=2, kernel_name, &is_first)
  │
  ├─ depth=0 (first dim A): iterate i=0,1
  │   └─ depth=1 (second dim B): iterate j=0,1
  │       ├─ is_first=1: Modify existing handle (ihdl=0), set A=1, B=x
  │       ├─ is_first=0: add_handle("stream"), set A=1, B=y
  │       ├─ is_first=0: add_handle("stream"), set A=2, B=x
  │       └─ is_first=0: add_handle("stream"), set A=2, B=y
```

**Key behaviors**:
- The first combination **modifies in-place** the existing handle (created by `--kernel`)
- Subsequent combinations call `tpb_driver_add_handle()` to create new handles
- Each combination applies all dimension values from **first to last** order

### 6.2 `apply_dim_value()` — Setting a Dimension Value onto a Handle

Location: `src/tpbcli-run.c:350`

```c
apply_dim_value(dim_val, index)
  ├── If string: tpb_driver_set_hdl_karg(parm_name, str_values[index])
  └── If numeric:
        ├── Integer: formatted as "%lld"
        └── Float:   formatted as "%.15g"
        → tpb_driver_set_hdl_karg(value_str)
```

`tpb_driver_set_hdl_karg()` performs:
1. Look up the parameter name in the current handle's `argpack`
2. Parse the string value based on the parameter's `ctrlbits` type code (int/uint/float/double/char)
3. Perform range validation (`TPB_PARM_RANGE`) or list validation (`TPB_PARM_LIST`)
4. Update `parm->value`

## 7. Differential Handling of `--kenvs-dim` and `--kmpiargs-dim`

| Dimension Type | Expansion Timing | Expansion Method |
|----------------|------------------|------------------|
| `--kargs-dim` | **Deferred** (on next `--kernel` or end of parsing) | Cartesian product; first combination modifies existing handle |
| `--kenvs-dim` | **Immediate** | Cartesian product; subsequent combinations call `tpb_driver_copy_hdl_from()` to copy kargs |
| `--kmpiargs-dim` | **Immediate** | Parses `['opt1','opt2']` format; subsequent combinations copy handle |

### 7.1 `--kenvs-dim` Specifics

When `expand_env_dim_handles()` expands, subsequent combinations:
1. `tpb_driver_add_handle()` creates a new handle
2. `tpb_driver_copy_hdl_from(orig_hdl_idx)` copies kargs and envs from the original handle
3. Then applies the current combination's env dimension values

This ensures every env dimension combination carries the same kargs base configuration.

### 7.2 `--kmpiargs-dim` Explicit List Syntax

```bash
--kmpiargs-dim "['-np 2','-np 4']"
```

Parsing flow:
1. `parse_kmpiargs_list()` parses the list `['-np 2','-np 4']`
2. For each combination:
   - First: modifies the existing handle
   - Subsequent: `add_handle()` + `copy_hdl_from()` + reset mpiargs + append combined value

Note: The `{...}` nesting syntax for `--kmpiargs-dim` has been removed. Use multiple `--kmpiargs-dim` options for Cartesian products.

## 8. Handle Creation and Parameter Inheritance

### 8.1 Parameter Initialization in `tpb_driver_add_handle()`

Location: `src/corelib/tpb-driver.c:979`

```c
tpb_driver_add_handle(kernel_name)
  ├── Look up kernel metadata
  ├── Allocate new handle (handle_list[nhdl], first handle at index 0)
  ├── Copy kernel info
  ├── Build argpack: copy default values from kernel->info.parms (no pseudo-handle override)
  ├── envpack cleared (empty)
  └── mpipack cleared (empty)
```

### 8.2 `tpb_driver_copy_hdl_from()` — Copying Existing Configuration

Location: `src/corelib/tpb-driver.c:1374`

Used when `--kenvs-dim` and `--kmpiargs-dim` expansion creates new handles:

```c
tpb_driver_copy_hdl_from(src_idx)
  ├── Copy argpack values for same-named parameters
  ├── Copy envpack (full replacement)
  └── Copy mpipack (full replacement)
```

Note: `--kargs-dim` expansion does **not** use `copy_hdl_from`: the first combination modifies the current handle in-place; subsequent combinations `add_handle` and carry only **kernel-registered defaults**, then apply each dimension's value (they do not automatically copy parameters modified via `--kargs` on a previous combination).

## 9. Complete Flow Example

```bash
tpbcli run --kernel stream --kargs ntest=20 \
  --kargs-dim 'stream_array_size=[32768,524288,3145728]'
```

(The `stream` kernel uses `stream_array_size`; other kernels use their own registered parameter names.)

### Execution Sequence:

```
1. tpb_register_kernel()
   → nhdl=0, handle_list=NULL

2. parse_run() iterates arguments:

   a. --kernel stream
      → tpb_driver_add_handle("stream")
         → handle_list[0]: stream kernel defaults
         → nhdl=1, ihdl=0
      → pending_kernel_name = "stream"

   b. --kargs ntest=20
      → handle_list[0].argpack ntest = 20

   c. --kargs-dim 'stream_array_size=[32768,524288,3145728]'
      → pending_dim_cfg = cfg (deferred expansion)

3. parse_run() ends, triggers remaining dim expansion:
   → expand_dim_handles(...)
      → Combination 0: modifies handle_list[0], stream_array_size=32768
      → Combination 1: add_handle → handle_list[1], kernel defaults + dim value 524288
      → Combination 2: add_handle → handle_list[2], kernel defaults + dim value 3145728
      → nhdl=3

4. tpb_driver_run_all()
   → Iterates handle_list[0..2]
```

### Final Handle State (Illustrative)

| Handle | Kernel | ntest | stream_array_size | Notes |
|--------|--------|-------|-------------------|-------|
| [0] | stream | **20** | **32768** | `--kargs` + first dim group |
| [1] | stream | 10 (kernel default) | **524288** | New handle; does not inherit `--kargs` from [0] |
| [2] | stream | 10 (kernel default) | **3145728** | Same as above |

To ensure every dim combination carries `ntest=20`, use multiple `--kargs-dim` options (e.g., `--kargs-dim ntest=[20,20,20] --kargs-dim stream_array_size=[...]`) or explicitly set via script / multiple `--kernel` invocations.

## 10. Key Data Structure Relationships

```
handle_list[0]  → First handle created by --kernel (modifiable in-place by first dim group)
handle_list[1]  → Second combination or second --kernel …
...

Each handle (tpb_k_rthdl_t) contains:
  ├── kernel:     Kernel metadata (info.name, info.parms defaults, info.outs)
  ├── argpack:    Runtime parameter array (initialized from kernel defaults, overridden by --kargs/--kargs-dim)
  │   └── args[]: tpb_rt_parm_t { name, value, ctrlbits, plims, nlims }
  ├── envpack:    Environment variables (set by --kenvs/--kenvs-dim)
  │   └── envs[]: tpb_env_entry_t { name, value }
  ├── mpipack:    MPI arguments (set by --kmpiargs/--kmpiargs-dim)
  │   └── mpiargs: char* (e.g. "-np 2 --bind-to core")
  └── respack:    Runtime outputs (populated during kernel execution)
      └── outputs[]: tpb_k_output_t { name, dtype, unit, n, p }
```

## 11. Parameter Precedence

For the **same** handle and **same** parameter name, approximate precedence (high to low):

1. **`--kargs-dim` expansion** — values written by `apply_dim_value()`
2. **Later `--kargs` within the same segment** — overrides earlier `--kargs`
3. **Kernel-registered defaults** — `tpb_k_add_parm()` default_val

`add_handle` no longer merges `_tpb_common` from a pseudo-handle. New handles produced by dim expansion start from kernel defaults, then apply dim values; they do not automatically copy fields modified via `--kargs` on a previous handle.
