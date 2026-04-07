# 1 编译 tpbcli

**1）基本编译命令**

对于具有成熟的开发或生产环境的系统，建议使用 CMake >= 3.16 构建 TPBench，以获得最佳兼容性、性能和持续集成功能。通过以下命令可以快速构建并测试 TPBench。

```
$ git clone https://github.com/KeyLiaoHPC/TPBench 
$ cd TPBench
$ mkdir build && cd ./build
$ cmake ..
$ make
$ ls
bin  CMakeCache.txt  CMakeFiles  cmake_install.cmake  etc  lib  log  Makefile
```

**2）SIMD 和并行化编译选项**

TPBench 支持通过 CMake 启用 SIMD（`TPB_USE_AVX512` 等）以及为已选中的 kernel 目标添加 OpenMP（`TPB_ENABLE_OPENMP`）。常用选项如下：

| CMake 选项 | 默认值 | 描述 |
| -------- | -------- | -------- |
| `-DTPB_USE_AVX512=ON` | OFF | 启用AVX-512指令集（x86_64） |
| `-DTPB_USE_AVX2=ON` | OFF | 启用AVX2指令集（x86_64） |
| `-DTPB_USE_KP_SVE=ON` | OFF | 启用ARM SVE指令集（aarch64） |
| `-DTPB_ENABLE_OPENMP=ON` | OFF | 为已选中的 kernel 目标添加 OpenMP（不决定编哪些内核） |
| `-DTPB_MPI_PATH=</path/to/mpi>` | 空 | 选中需 MPI 的内核时的 MPI 根目录（空则自动探测） |

示例：在 x86_64 平台上启用 AVX-512 指令集编译
```bash
$ cmake -DTPB_USE_AVX512=ON ..
$ make
```

示例：在 ARM 平台上启用 SVE 指令集编译
```bash
$ cmake -DTPB_USE_KP_SVE=ON ..
$ make
```

注意：SIMD 选项需要编译器和目标平台支持相应的指令集。启用 AVX-512 或 AVX2 时，建议同时添加 `-march=native` 编译选项以获得最佳性能。

# 2 使用 tpbcli 
## 2.1 简介
`tpbcli` 的基本格式：
```bash
tpbcli <subcommand> <options>
```
顶层子命令（括号内为短别名）：**`run`（`r`）**、**`benchmark`（`b`）**、**`database`（`db`）**、**`kernel`（`k`）**、**`help`（`h`）**。顶层使用 **`--help`** 或 **`-h`** 可查看完整 CLI 说明。

- **`tpbcli run`**：用于运行一个或多个 TPBench 内核，支持设置运行时参数与可变参数维度扫描；可为每个内核传递命令行参数、环境变量或 MPI 参数。
- **`tpbcli benchmark`**：运行预定义评测套件（参数、计分规则与输出）。
- **`tpbcli database`** / **`tpbcli db`**：查看工作区内 rafdb 记录 —— **`list`** / **`ls`**（近期 tbatch 表）或 **`dump`**，且 **`dump`** 须且仅能指定一个选择项（**`--id`**、**`--tbatch-id`**、**`--kernel-id`**、**`--task-id`**、**`--score-id`**、**`--file`**、**`--entry`**）。**`tpbcli database --help`** 可查看子命令与 dump 选项摘要；**`tpbcli database dump --help`** 查看 dump 全部选项。单独 **`tpbcli database`**（无 `list`/`dump`）会报错。
- **`tpbcli kernel`**：刷新工作区内核元数据后执行 **`list`** / **`ls`**，列出已注册的 PLI 内核（与旧版顶层 `list` 不同）。
- **`tpbcli help`**：帮助文档。

屏幕上的输出结果会同时输出到log目录下。

## 2.2 tpbcli run

### 2.2.1 基础格式

`tpbcli run` 的命令行格式如下所示，通过搭配 `kargs[_dim]`/`kenvs[_dim]`/`kmpiargs[_dim]` 选项，可以运行多个评测内核的评测，并为不同评测内核创建不同的参数组合，从而使用一条命令运行多个评测内核的多维度可变参数测试。在下方命令格式中，所有尖括号“\<\>”选项均需要被实际使用的选项名称替换。注意，使用`--kargs-dim`、`--kenvs-dim`和`--kmpiargs-dim`时，选项需要用一对单引号或双引号包围。
``` bash
tpbcli run <tpbench_options> \
[--kernel <kernel_name> \
[--kargs/--kargs-dim <opts> | --kenvs/--kenvs-dim <opts> | --kmpiargs/--kmpiargs-dim <opts>]]
```

**规则：** `--kargs`、`--kargs-dim`、`--kenvs`、`--kenvs-dim`、`--kmpiargs`、`--kmpiargs-dim` 均必须出现在其所作用的 **`--kernel` 之后**（作用于命令行上最近一个 `--kernel`）。命令行上不再支持在第一个 `--kernel` 之前设置“默认”参数。
\<tpbench_options\>支持的选项包括：
- `-P`: 选择PLI集成内核（默认，保留以向后兼容）。
- `--timer`: 选择名为\<timer_name\>的计时方法，默认为clock_gettime。
- `--outargs`: 日志及数据输出格式设置
    - `unit_cast=[0/1]`: 是否进行自动单位转换，默认为0，不进行转换。
    - `sigbit_trim=<x>`: 限制输出的数据的有效位数，整数部分超出位数会使用科学计数法表示。

### 2.2.2 运行基础评测

**1）选择评测内核和内核参数**（`--kernel`, `--kargs`）

`--kernel` 定义名为 `<kernel_name>` 的待测内核。其后出现的 `--k*` 选项作用于该内核，直至下一个 `--kernel` 或命令行结束。每出现一次 `--kernel` 会创建一个运行句柄（维度展开可为同一内核名再增句柄）。一个 `--kargs` 可带多个参数，逗号分隔；若值内含逗号请加引号。

语法：`--kernel <kernel_name> --kargs '<key1>=<value1>,<key2>=<value2>,<key3>="<v3>,<with>,<complex>,<section>",...'`

在同一 `--kernel` 段内，`--kargs`、`--kenvs`、`--kmpiargs` 可出现多次；带 `_dim` 的选项限制见 2.2.3。同一参数名多次出现时，以最后一次为准。

`--kargs` / `--kenvs` / `--kmpiargs` 的每项为 `<key>=<value>`。TPBench 按该内核注册的参数做合法性检查。对同一句柄，若有 `--kargs-dim` / `--kenvs-dim` / `--kmpiargs-dim` 展开，其值覆盖同段的普通 `--kargs` / `--kenvs` / `--kmpiargs`；同段内靠后的选项覆盖靠前的。

注意：内核内部可能对参数再加工（如对齐），注册默认值与最终使用值可能不一致。例如 `total_memsize=128` KiB、double 精度 triad 下，每数组可能为 **131064** 字节而非 131072。每轮结束后终端会打印内核实际使用的参数。

示例1：运行 triad，总内存 128KiB
```bash
$ tpbcli run -k triad --kargs total_memsize=128
```

示例2：两轮 triad：第一轮 100 次循环、128KiB；第二轮 100 次循环、256KiB
```bash
$ tpbcli run -k triad --kargs ntest=100,total_memsize=128 -k triad --kargs ntest=100,total_memsize=256
```

示例3：先 triad 再 pchase：triad 128KiB、100 次循环；pchase 1000 次循环（具体参数名以 pchase 内核为准）。
```bash
$ tpbcli run -k triad --kargs total_memsize=128,ntest=100 -k pchase --kargs ntest=1000
```

使用 `--kernel -l` 可以列出目前可用的评测内核，使用 `--kernel <foo> --kargs -l` 可以列出 `<foo>` 内核支持的命令行输入参数。

### 2.2.3 运行可变参数评测

配置可变参数评测时，TPBench 按照预定义的取值序列逐个运行测试：每个取值都会执行一次内核，从而得到参数沿坐标轴变化时的结果（例如性能随某参数变化的曲线）。当指定一个评测内核 `--kernel <foo>` 时，在 `foo` 内核中可以通过 `--kargs` 配置的任一参数，均可以通过 `--kargs-dim` 配置为可变参数。若可变参数名与 `--kargs` 中配置的参数名重合，则 `--kargs` 中的同名参数将无效。

目前，`tpbcli run` 支持配置显式列表和递推序列。若需要遍历多个维度，则可以嵌套多个参数序列。

**1）显式列表**

对于指定评测内核的一个参数，生成一个显式指定元素的集合，依次使用集合中的每个元素运行评测。语法如下所示，对参数 `<parm_name>`，生成集合，包含元素 `a`、`b`、`c`……

语法：`--kargs-dim '<parm_name>=[a, b, c, ...]'`

示例：运行 `triad` 内核，设置总内存容量 256KiB，每轮测试运行 100 次循环。配置数据类型 `dtype` 为可变参数，该参数为显式列表序列，包含 `double`、`float`、`iso-fp16`。TPBench 将轮流使用上述 3 种变量格式，共运行 3 轮 triad 测试。

```
$ tpbcli --kernel triad --kargs ntest=100,total_memsize=256 --kargs-dim 'dtype=[double,float,iso-fp16]'
```

**2）递推序列**

对于指定评测内核的一个参数，生成一个递推参数序列。初值 `st`，使用计算 `<op>`，基于上一次测试的参数值 `@` 计算出新参数值。上述参数配置中，符号 `@` 表示递推变量；`op` 表示操作符，目前支持 add、sub、mul、div 和 pow；`st` 表示 `parm_name` 参数首次测试的值；`min`、`max` 和 `nlim` 分别表示递推结果的最小值、最大值和递推步数限制。当递推结果超出 `[min, max]` 区间，或递推步数高于 `nlim` 时，递推将终止。`nlim` 设置为 0 时，不限制递推步数。

语法：`--kargs-dim <parm_name>='<op>(@,x)(st,min,max,nlim)'`

示例：运行 `triad` 内核，每轮执行 100 个循环，轮流将 `total_memsize` 设置为 `16`、`32`、`64`、`128`，共运行 4 轮测试。

```
$ tpbcli --kernel triad --kargs ntest=100 --kargs-dim 'total_memsize=mul(@,2)(16,16,128,0)'
```

**3）多参数笛卡尔积**

要同时遍历多个参数的组合，使用**多个** `--kargs-dim` 选项（每个参数一个）。TPBench 会自动构建所有维度的**笛卡尔积**（例如两个长度分别为 2 和 3 的列表会产生 6 次运行）。单个 `--kargs-dim` 字符串中的 `{...}` 嵌套语法**不再支持**。

示例：运行 `triad` 内核，每轮执行 100 个循环，轮流使用 `double`、`float` 和 `iso-fp16` 这三种数据格式，并对每种格式轮流将 `total_memsize` 设置为 `16`、`32`、`64`、`128`，共运行 12 轮测试（3 种数据类型 × 4 种内存大小）。

```
$ tpbcli run --kernel triad --kargs ntest=100 \
    --kargs-dim dtype=[double,float,iso-fp16] \
    --kargs-dim total_memsize=mul(@,2)(16,16,128,0)
```

### 2.2.4 设置计时方法

TPBench 默认使用 `clock_gettime` 获取 `CLOCK_MONOTONIC_RAW` 时钟（Linux 内核版本不低于 2.6.28）。也可以使用 `--timer <timer_name>` 设置其它计时方法，使用 `tpbcli run --timer -l` 显示目前支持的计时方法。`<timer_name>` 可以选择如下计时方法：

| `<timer_name>` | 单位（缩写） | 描述 |
| -------- | -------- | -------- |
| clock_gettime | nanosecond(ns) | POSIX clock_gettime with CLOCK_MONOTONIC_RAW |
| tsc_asym | cycle(cy) | A RDTSCP-based timing method provided by Gabriele Paoloni in Intel White Paper 324264-001 |
| cpu_clk_p | cycle(cy) | Count the CPU_CLK_UNHALTED.THREAD_P event. |
| cntvct | tick(tk) | The CNTVCT counter. Only available on the aarch64 platform. |
| pncctr | cycle(cy) | The CNTVCT counter. Only available on the aarch64 platform. |

一种计时方法定义了时钟源、读取时钟源的函数、数据格式、存储格式、存储方法和时钟计算方法。因此，相同的评测内核和参数在使用不同的计时方法时，可能得到不一样的输出结果和输出单位。

### 2.2.5 设置传入评测内核的环境变量

**1) 设置单个或多个环境变量**

示例：运行 triad 时，循环 100 次，内存大小为 128KiB，使用 16 个 OpenMP 线程，每 4 个线程为一组，分别绑定至 CPU 核心 0-3、4-7、8-11、12-15。

```
$ tpbcli --kernel triad --kargs ntest=100,total_memsize=128 --kenvs OMP_NUM_THREADS=16,OMP_PLACES="{0:4}:4:4"
```


### 2.2.6 设置 MPI 运行参数

**1) 设置 MPI 参数**

`--kmpiargs` 选项接受一个字符串，该字符串将原样传递给 `mpirun`。字符串应当用单引号或双引号包裹。

语法：`--kmpiargs '<mpi_args_string>'`

示例：运行 stream_mpi 内核，测试 100 次迭代，所有 rank 的聚合数组大小为 43690 个元素，使用 2 个 MPI 进程。

```bash
$ tpbcli run --kernel stream_mpi --kargs ntest=100,stream_array_size=43690 --kmpiargs ' -np 2'
```

在同一 `--kernel` 之后可多次指定 `--kmpiargs`，各段用空格拼接。

**2) 可变 MPI 参数**

`--kmpiargs-dim` 支持显式列表和嵌套列表格式，用于扫描不同的 MPI 配置。

语法：`--kmpiargs-dim "['opt1', 'opt2', ...]{['opta', 'optb', ...]}"`

示例1：运行 stream_mpi 内核，扫描 MPI 进程数从 1 到 4。

```bash
$ tpbcli run --kernel stream_mpi --kargs ntest=100,stream_array_size=43690 \
    --kmpiargs '--bind-to core' \
    --kmpiargs-dim "['-np 1', '-np 2', '-np 4']"
```

示例2：使用嵌套列表扫描进程数和绑定策略。

```bash
$ tpbcli run --kernel stream_mpi --kargs ntest=100,stream_array_size=43690 \
    --kmpiargs '--bind-to core' \
    --kmpiargs-dim "['-np 2', '-np 4']{'--bind-to core', '--bind-to socket'}"
```

上述命令将生成 4 种组合：
- `-np 2 --bind-to core`
- `-np 2 --bind-to socket`
- `-np 4 --bind-to core`
- `-np 4 --bind-to socket`

**3) 命令行分析**

TPBench 执行 PLI 内核时，将完整命令行打印至终端，便于调试和分析。命令行格式为：

```
TPBENCH_TIMER=<timer> [ENV=VAL ...] [mpirun <mpiargs>] <exec_path> <timer> <params...>
```

注意：MPI 参数直接传递给 `mpirun`，TPBench 不进行验证。如果 `mpirun` 子进程失败，将报告错误。

## 2.3 tpbcli database

同义写法：**`tpbcli database`**、**`tpbcli db`**。

必须再指定子命令：**`list`**（别名 **`ls`**）或 **`dump`**。

- **`tpbcli db list`** — 打印工作区索引中近期 tbatch 表（与 **`database list`** 相同）。
- **`tpbcli db dump`** — 须且仅能指定以下之一：**`--id`**、**`--tbatch-id`**、**`--kernel-id`**、**`--task-id`**、**`--score-id`**、**`--file`** *路径*、**`--entry`** *名称*（详见 **`dump --help`**）。各选择项互斥。

示例：

```bash
$ tpbcli db list
$ tpbcli database dump --tbatch-id <40位十六进制>
```
