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
| `-DTPB_MPI_PATH=</path/to/mpi>` | 空 | 选中 MPI kernel 时的 MPI 根目录（仅 kernel .so 链接 MPI，libtpbench 不链接；空则自动探测） |

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

**3）安装到 TPB_HOME**

将可执行文件、库与模板安装到 **`TPB_HOME`**（TPBench 安装根目录）。若环境中未设置 **`TPB_HOME`**，安装时的 rafdb 同步步骤默认使用 **`$HOME/.tpbench`** 并打印提示信息。

```bash
export TPB_HOME="${TPB_HOME:-$HOME/.tpbench}"
cmake --build build
cmake --install build --prefix "${TPB_HOME}"
```

Post-build 会将 kernel 编译元数据与构建期运行时环境快照写入 **构建目录**（`build/rafdb/`）。**全量** **`cmake --install`**（非 **`--component tpbench_kernels`**）时，仅将 **`rafdb/kernel/`** 与 **`rafdb/runtime_environment/`** 拷贝到 **`$TPB_HOME/rafdb/`**，并合并 **`runtime_environment.base_id`** 到 **`$TPB_HOME/etc/config.json`**。不会拷贝 **`rafdb/task/`**、**`rafdb/task_batch/`**、**`rafdb/log/`** 中的运行记录。

| 情况 | 行为 |
| -------- | -------- |
| `$TPB_HOME/rafdb/` 无 `.tpbe`/`.tpbr` 数据 | 从构建目录拷贝元数据 |
| `$TPB_HOME/rafdb/` 某 metadata 域已有数据 | 跳过该域覆盖，仅拷贝空域。使用 **`TPB_INSTALL_RAFDB=YES`** 可备份并替换 |
| 强制或禁用 | **`TPB_INSTALL_RAFDB=YES`** 或 **`-DTPB_INSTALL_RAFDB=YES`** 始终备份并拷贝；**`NO`** 永不拷贝 |

安装步骤**不读取、不修改 `TPB_WORKSPACE`**。运行 benchmark 时请单独设置 **`TPB_WORKSPACE`** 存放评测结果。**`cmake --install --prefix`** 应与 **`TPB_HOME`** 一致；不一致时会打印警告。

# 2 使用 tpbcli 
## 2.1 简介
`tpbcli` 的基本格式：
```bash
tpbcli <subcommand> <options>
```
顶层子命令（括号内为短别名）：**`run`（`r`）**、**`benchmark`（`b`）**、**`database`（`db`）**、**`runtime environment`（`rtenv`）**、**`kernel`（`k`）**、**`help`（`h`）**。顶层使用 **`--help`** 或 **`-h`** 可查看完整 CLI 说明。

- **`tpbcli run`**：用于运行一个或多个 TPBench 内核，支持设置运行时参数与可变参数维度扫描；可为每个内核传递命令行参数、环境变量或 MPI 参数。
- **`tpbcli benchmark`**：运行预定义评测套件（参数、计分规则与输出）。
- **`tpbcli database`** / **`tpbcli db`**：查看工作区内 rafdb 记录 —— **`list`** / **`ls`**（近期索引表）或 **`dump`**（须指定 **domain**，再加 **`-i`/`--id`** 导出单条 `.tpbr`，或 **`-e`** 导出 `.tpbe` 索引）。**`tpbcli database --help`** 可查看子命令与 dump 选项摘要；**`tpbcli database dump --help`** 查看 dump 全部选项。单独 **`tpbcli database`**（无 `list`/`dump`）会报错。
- **`tpbcli rtenv`**：创建、浏览和加载运行时环境记录，记录应用/库版本和显式环境变量。子命令：**`new`**、**`list`**、**`show`**、**`load`**。
- **`tpbcli kernel`**：管理工作区内的 kernel 编译历史 —— **`list`** / **`ls`**（扫描并注册 PLI 内核）、**`get`**（只读查询）、**`set`**（写入 metadata），以及构建系统内部使用的 **`backup-inactive`**。
- **`tpbcli help`**：帮助文档。

屏幕上的输出结果会同步写入日志文件，路径：`<workspace>/rafdb/log/tpbrunlog_*.log`。

## 2.2 tpbcli run

### 2.2.1 基础格式

`tpbcli run` 的命令行格式如下所示，通过搭配 `kargs[_dim]`/`kenvs[_dim]` 与可选的 `--wrapper` 链，可以运行多个评测内核的评测，并为不同评测内核创建不同的参数组合，从而使用一条命令运行多个评测内核的多维度可变参数测试。在下方命令格式中，所有尖括号“\<\>”选项均需要被实际使用的选项名称替换。注意，使用 `--kargs-dim` 和 `--kenvs-dim` 时，选项需要用一对单引号或双引号包围。
``` bash
tpbcli run <tpbench_options> \
[--wrapper <app> [--wrapper-args '<args>']]... \
[--kernel <kernel_name> [-og] \
[--kargs/--kargs-dim <opts> | --kenvs/--kenvs-dim <opts>] \
[--wrapper <app> [--wrapper-args '<args>']]...]...
```

**规则：** `--kargs`、`--kargs-dim`、`--kenvs`、`--kenvs-dim` 均必须出现在其所作用的 **`--kernel` 之后**（作用于命令行上最近一个 `--kernel`）。命令行上不再支持在第一个 `--kernel` 之前设置“默认”参数。

**Wrapper 规则：** 第一个 `--kernel` 之前的 `--wrapper` / `--wrapper-args` 组成**全局**链，默认加在每个 kernel 前；某 kernel 组使用 `-og` / `--override-global` 时跳过全局链，保留该 kernel 的局部 wrapper。Wrapper 按顺序链接，后面的不会替换前面的。

**内核发现：** `tpbcli run` 仅加载命令行 `--kernel` / `-k` 指定的内核，**不会**扫描整个 `lib/libtpbk_*.so`。若内核名不存在或 `.so` 无法加载，会输出 `Kernel <name> not found. Use \`tpbcli kernel list\` to show kernel lists.`。请使用 **`tpbcli kernel list`** 查看已安装内核。找到已注册内核时，run 会输出 `Kernel <name> found, KernelID: <id>`（首次注册时为 `New kernel found, add to kernel records.`）。
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

在同一 `--kernel` 段内，`--kargs`、`--kenvs` 可出现多次；带 `_dim` 的选项限制见 2.2.3。同一参数名多次出现时，以最后一次为准。

`--kargs` / `--kenvs` 的每项为 `<key>=<value>`。TPBench 按该内核注册的参数做合法性检查。对同一句柄，若有 `--kargs-dim` / `--kenvs-dim` 展开，其值覆盖同段的普通 `--kargs` / `--kenvs`；同段内靠后的选项覆盖靠前的。

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


### 2.2.6 PLI Wrapper 链

使用 `--wrapper` 和 `--wrapper-args` 在内核入口命令前按顺序插入可执行 wrapper。MPI 运行使用 `--wrapper mpirun --wrapper-args '...'`，不再使用专用 MPI CLI 选项。

**1) 全局与 per-kernel wrapper**

- 第一个 `--kernel` **之前**的 wrapper 作用于所有 kernel（全局链）。
- 某个 `--kernel` **之后**的 wrapper 仅作用于该 kernel（局部链），接在全局链之后。
- 在某 kernel 组使用 `-og` / `--override-global` 可跳过全局链，保留局部 wrapper。

语法：

```bash
--wrapper <app> [--wrapper-args '<args_str>']
```

示例：全局 `numactl`，第二个 kernel 再使用 `mpirun`：

```bash
$ tpbcli run --wrapper numactl --kernel stream --kernel stream_mpi \
    --wrapper mpirun --wrapper-args '-np 20'
```

示例：第二个 kernel 忽略全局 wrapper：

```bash
$ tpbcli run --wrapper numactl --kernel stream --kernel stream_mpi -og \
    --wrapper mpirun --wrapper-args '-np 20'
```

示例：MPI kernel：

```bash
$ tpbcli run --kernel stream_mpi --kargs ntest=100,stream_array_size=43690 \
    --wrapper mpirun --wrapper-args '-np 2'
```

同一 `--wrapper` 后可多次指定 `--wrapper-args`，各段用空格拼接。未先指定 `--wrapper` 时使用 `--wrapper-args` 会被拒绝。

**2) 命令行分析**

TPBench 执行 PLI 内核时，将完整命令行打印至终端，便于调试和分析。命令行格式为：

```
TPBENCH_TIMER=<timer> [ENV=VAL ...] [wrapper_app wrapper_args ...] <kernel_entry> <timer> <params...>
```

`<kernel_entry>` 为启动内核的解析后命令（共享库 PLI 内核通常为 `tpbcli-pli-launcher` 加 kernel `.so` 路径）。Wrapper 参数由 wrapper 可执行文件解释，TPBench 不验证。

## 2.3 tpbcli database

同义写法：**`tpbcli database`**、**`tpbcli db`**。

必须再指定子命令：**`list`**（别名 **`ls`**）或 **`dump`**。

- **`tpbcli db list`** — 列出 rafdb 索引记录（默认：tbatch 域、最近 20 条）。可通过选项选择域与条数。
- **`tpbcli db dump`** — 须指定 **domain**（**`-dT`/`-dt`/`-dk`/`-dr`** 或 **`--domain`**），且 **`-i`/`--id`**（单条 `.tpbr`）与 **`-e`**（`.tpbe` 索引；可选 **`-n`/`-N`**，默认最近 20 条）二选一。详见 **`dump --help`**。

### `db list` 选项

```
tpbcli db list [-dT|-dt|-dk|-dr | --domain <tbatch|task|kernel|runtime_environment>] [-n <N> | -N <N>]
```

默认：**`-dT`**（tbatch）与 **`-n 20`**（最近 20 条）。

| 选项 | 含义 |
|------|------|
| `-dT` | tbatch 域（默认） |
| `-dt` | task 域（仅入口点；MPI capsule，不含各 rank 行） |
| `-dk` | kernel 域 |
| `-dr` | runtime_environment 域 |
| `--domain <name>` | 与 `-dT`/`-dt`/`-dk`/`-dr` 等价（`tbatch`、`task`、`kernel` 或 `runtime_environment`；别名 `rtenv`） |
| `-n <N>` | 最近 *N* 条（新的在上） |
| `-N <N>` | 最旧 *N* 条（旧的在先） |

域选择项互斥；**`-n`** 与 **`-N`** 互斥。

记录 ID 显示为 6 位十六进制前缀加 `*`（如 `b12a23*`），与 `tpbcli kernel ls` 一致。列宽按终端比例分配（`tpblog_printf_c`，与 kernel list 相同）。

**tbatch 列：** Start Time (UTC)、Type、Duration(s)、NTask、NKernel、NScore、TBatch ID

**task 列：** Start Time (UTC)、Kernel、Exit、Duration(s)、Handle、Task ID、TBatch ID

**kernel 列：** Kernel Name、Active、NParm、NMetric、Build Time (UTC)、Kernel ID

**runtime_environment 列：** ID、Name、Hostname、Created UTC、NTask、NTBatch、NApp、NEnv、Note

示例：

```bash
$ tpbcli db list
$ tpbcli db list -dt -n 10
$ tpbcli db list --domain kernel -N 5
$ tpbcli db list -dr -n 10
$ tpbcli db list --domain runtime_environment
```

### `db dump` 选项

```
tpbcli db dump [-dT|-dt|-dk|-dr | --domain <tbatch|task|kernel|runtime_environment|rtenv>]
              (-i|--id <id> | -e)
              [-n <N> | -N <N>]
```

**必填：** 一个 domain 选择项（无默认值）。**必填：** `-i`/`--id` 或 `-e`（互斥）。

| 选项 | 含义 |
|------|------|
| `-dT` | tbatch 域 |
| `-dt` | task 域（`-e` 时仅入口点） |
| `-dk` | kernel 域 |
| `-dr` | runtime_environment 域 |
| `--domain <name>` | 与 `-dT`/`-dt`/`-dk`/`-dr` 等价（`tbatch`、`task`、`kernel`、`runtime_environment`；别名 `rtenv`） |
| `-i` / `--id <id>` | 导出所选域的一条 `.tpbr`。tbatch/kernel/task 为 SHA-1 十六进制（4–40 位）；rtenv 为十进制 |
| `-e` | 导出 `.tpbe` 索引行（CSV 风格），而非单条 `.tpbr` |
| `-n <N>` | 配合 `-e`：最近 *N* 条（默认 20） |
| `-N <N>` | 配合 `-e`：最旧 *N* 条 |

域选择项互斥；**`-i`/`--id`** 与 **`-e`** 互斥；**`-n`** 与 **`-N`** 互斥。

**Record Data** 按 header 的 `type_bits` 格式化：`TPB_STRING_T` 以文本输出（每格 `dimsizes[0]` 字节）；整型与浮点以十进制输出；20 字节 ID 仍为十六进制。task 的 `environment_variable_*` 因此显示为可读字符串与整数计数（key 与扁平 value 段以 `:` 连接；按 `count` 解码各 key）。

示例：

```bash
# 单条 .tpbr
tpbcli db dump -dT -i <40位十六进制>
tpbcli db dump -dt -i <CapsuleID>
tpbcli db dump -dr -i 0

# .tpbe 索引（默认最近 20 条）
tpbcli db dump -dk -e
tpbcli db dump --domain runtime_environment -e -n 10
tpbcli db dump -dt -e -N 5
```

## 2.4 tpbcli rtenv

**`rtenv`** 子命令管理 rafdb 的 `runtime_environment` domain，用于记录
Environment Modules 风格的软件栈和环境变量。

TPBench 强制要求始终存在一个激活 RTEnv。主工程每次链接 **`tpbcli`** 时，
post-build 会执行 **`tpbcli rtenv init-base`**，追加一条**根环境快照**
（`inherit_from == 0`），写入编译进程的真实 hostname、时间戳及全部 `environ`
变量（`on_set=ignore`、`on_get=ignore`）。首次快照为 `id == 1`、name `base`；
再次编译则生成 `base_1`、`base_2` … 并更新 `etc/config.json` 的 **`base_id`**。
此后每次 `tpbcli rtenv new`（别名 `create`）都必须继承当前激活的 RTEnv。

### 2.4.1 创建运行时环境

```bash
tpbcli rtenv new -n gcc-openmpi -N 'GCC + OpenMPI' \
  -app -n gcc -v 13.2 -N compiler \
  -app -n openmpi -v 5.0.3 -N mpi \
  -evp -k PATH -v /opt/gcc/bin \
  -eva -k LD_LIBRARY_PATH -v /opt/gcc/lib64 \
  -evo -k SOME_VAR -v ''
```

新环境继承当前激活 RTEnv（可用 `--inherit-from <id|name>` 显式指定父环境），只记录
相对父环境的增量。命令行变量选项：`-evo|--env-var-overwrite`、
`-evp|--env-var-prepend`、`-eva|--env-var-append`（各跟 `-k` / `-v`）。清空变量用
`-evo -k KEY -v ''`。`on_get` 在 kernel init 采集时生效；模板文件用
`var=<on_get>:<on_set>:...` 行。

不带应用或变量选项时，`new` 会打开编辑器；`-o <file>` 导出当前 active 合并视图
（不写入 rafdb）；`-f <file>` 从模板文件创建记录。

模板格式（`-o` / `-f`）：

```text
inherit_from=<id>
name=<environment_name>
note=<note>
application=<name>:<version>:<note>
var=<on_get>:<on_set>:<key>[:<value_segment>...]
```

`application=` / `var=` 内冒号按引号感知切分；同一 key 的 value 段用 `:` 连接。
RTEnv 模板 key 不得含 `:`；task 快照 value 段内不得含 `:`（段由采集时按 `:` 切分）。

### 2.4.2 浏览运行时环境

```bash
tpbcli rtenv list
tpbcli rtenv show -i 1
tpbcli rtenv show --name gcc-openmpi
```

`list` 会先显示当前激活的 runtime environment ID；无法解析时显示 `N/A`。
`show` 沿 `inherit_from` 链合并后展示 Applications (merged) 与 Environment
Variables (merged) 表，列含 `On_set`、`On_get`、`Key`、`Value`；Value 列定宽换行且
无连字符折行。

### 2.4.3 加载运行时环境

`tpbcli` 子进程不能直接修改父 shell。要让环境变量进入当前 shell，请执行：

```bash
eval "$(tpbcli rtenv load -i 1)"
```

或：

```bash
source <(tpbcli rtenv load --name gcc-openmpi)
```

`load` 按各变量 `on_set` 在 stdout 输出 `export`（`ignore` 不输出；overwrite
空值输出 `export KEY=''`）；日志到 stderr。**运行期激活状态只由 `$TPB_RTENV_ID`
承载**（`load` 不写回 `config.json`）。

之后 `tpbcli run` / `benchmark` 按 `$TPB_RTENV_ID` 解析激活环境（未设置时回退
基础环境并告警），回写 RTEnv 的 `ntbatch`/`ntask` 计数，并在每条 task 写入三条
环境快照 header：`environment_variable_key`、`environment_variable_count`、
`environment_variable_value`（init 时按 `on_get` 采集全 `environ`）。`--kenvs` 在
active 合并 RTEnv 中有声明时按该条 `on_set` 合成后注入，否则直接 `setenv`，均进入
上述三条 header。

## 2.5 tpbcli kernel

**`kernel`** 子命令管理 rafdb 中的 Kernel 域记录：已注册参数、指标、编译 metadata 以及每个 KernelID 变体的 **`active`** 状态。

### 2.5.1 列出已注册内核

```bash
tpbcli kernel list
# 别名：tpbcli kernel ls
```

扫描 `lib/libtpbk_*.so`，注册新 KernelID，并打印 **Kernel**、**KernelID**、**Tags**、**Description** 四列表格。已编译内核在前（加载顺序）；**`$TPB_HOME/src/kernels/kernel_list.cmake.in`**（及扫描到的 `tpbk_*.c` 入口）中尚未安装的内核，KernelID 列显示 **`N/A`**。Tags 来自 registry 文件，而非 rafdb 记录。

### 2.5.1a 内核源 registry

CPU 内核名称、标签与源码路径在 **`src/kernels/kernel_list.cmake.in`** 中声明（安装于 **`$TPB_HOME/src/kernels/`**）：

```text
# 格式：NAME|TAGS|PATH
stream|default,bandwidth|simple
stream_mpi|bandwidth,mpi|streaming_memory_access_mpi
```

**`PATH`** 相对于 `src/kernels/`，目录内须含 **`tpbk_<NAME>.c`**。特殊链接库与构建条件（如 MPI）仍在 **`cmake/TPBenchKernelRegistry.cmake`** 的 **`TPB_CPU_KERNEL_LINK_DEFS`** 中配置。

### 2.5.2 查询 kernel 记录（只读）

```bash
tpbcli kernel get --kernel <name>
tpbcli kernel get -v --kernel <name>    # 所有变体，从新到旧
```

**`get` 不会扫描 `.so`，也不会触发注册。** 仅读取 `rafdb/kernel/kernel.tpbe` 及对应 `.tpbr`。不带 **`-v`** 时显示该名称下最新且 **active** 的记录；若无 active 记录，则显示最新一条并标明 inactive。

### 2.5.3 设置编译 metadata

```bash
tpbcli kernel set --kernel <name> \
  --key <section>.<subkey> '<value>' \
  [--key <section>.<subkey> '<value>' ...]
```

如需要则注册指定内核（对当前 `libtpbk_<name>.so` 求哈希），然后 patch metadata 头部。支持的 section：**`variation`**、**`compilation`**、**`dependency`**。payload 为 `key=value` 文本（每行一对）。

示例：

```bash
tpbcli kernel set --kernel stream \
  --key compilation.compiler.id 'GNU' \
  --key dependency.tpbench 'libtpbench.so'

# 以 '-' 开头的值须紧跟 --key（不会被当作 CLI 选项）：
tpbcli kernel set --kernel stream --key compilation.kernel_cflags -O3
```

当 KernelID 已存在时，除非环境变量 **`TPB_K_OVERRIDE=1`**（或 `true`/`yes`），否则 **`set` 会跳过更新** 并打印 warning。在 **`kernel list`** 时重新注册未变化的 `.so` 同样受此约束。**`tpbcli run`** 不更新 kernel metadata，仅报告是否找到指定 kernel。

### 2.5.4 通过 CMake 记录编译历史

**`TPB_RECORD_KERNEL_COMPILE_HISTORY=ON`**（默认）时，每个已构建的 kernel 目标会在 post-build 阶段调用 **`tpbcli kernel set`** 写入 **`variation`**、**`compilation`**、**`dependency`**。构建时请设置 **`TPB_WORKSPACE`**，使记录写入预期工作区：

```bash
export TPB_WORKSPACE=$HOME/my-tpbench-ws
cmake -B build-o2 -DTPB_KERNELS=stream -DTPB_KERNEL_CFLAGS=-O2
TPB_WORKSPACE=$HOME/my-tpbench-ws cmake --build build-o2 --target tpbk_stream

cmake -B build-o3 -DTPB_KERNELS=stream -DTPB_KERNEL_CFLAGS=-O3
TPB_WORKSPACE=$HOME/my-tpbench-ws cmake --build build-o3 --target tpbk_stream

tpbcli kernel get -v --kernel stream
```

用相同编译选项重建会产生相同 KernelID；除非设置 **`TPB_K_OVERRIDE=1`**，否则不会覆盖已有 metadata。

替换 `lib/libtpbk_<name>.so` 前，构建系统会将旧文件移至 **`lib/inactive/libkernel_<name>_<kernel_id>.so_bak`**，并将旧 KernelID 标为 **`active=0`**。dynloader 仅扫描 `lib/libtpbk_*.so`（非递归），备份文件不会被加载。

### 2.5.5 初始化树外 kernel 工程

模板位于 **`${TPB_HOME}/etc/cmake/kernel/`**。

```bash
tpbcli kernel init --dir ./mykern --kernel mykern
```

### 2.5.6 构建 kernel

```bash
# 从 registry 构建（--dir 默认为 TPB_HOME）：
tpbcli kernel build --kernel stream
tpbcli kernel build --kernel 'stream,triad'
tpbcli kernel build --kernel-tag bandwidth

# 树外工程（显式源码目录）：
tpbcli kernel build --dir ./mykern --kernel mykern \
  [--ldflags "-Wl,--as-needed"] [--cflags "-O2"]
```

**`--kernel`** 与 **`--kernel-tag`** 互斥且必须指定其一；均支持逗号分隔，可用单/双引号包裹。**`--dir`** 默认为 **`TPB_HOME`**；默认时按 **`kernel_list.cmake.in`** 解析 **`$TPB_HOME/src/kernels/<PATH>`**。多内核顺序构建，逐个输出 PASS/FAIL 及汇总。 **`--ldflags`** 映射为 **`TPB_KERNEL_LDFLAGS`** 并写入 **`compilation.kernel_ldflags`**。
