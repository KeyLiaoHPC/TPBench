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
CMakeCache.txt  CMakeFiles  cmake_install.cmake  libtpbench.so  libtriad.so  Makefile  tpbcli
```

# 2 使用 tpbcli 
## 2.1 简介
`tpbcli` 的基本格式：
```bash
tpbcli <subcommand> <options>
```
支持 2 个子命令：`run` 和 `benchmark`。
- `tpbcli run`：用于运行一个或多个 TPBench 内核，支持设置运行时参数，进行可变参数维度扫描。支持通过 TPBench 框架为每个待测内核传递运行时命令行参数、环境变量或 MPI 运行时参数。
- `tpbcli benchmark`（开发中）：运行预定义的评测套件，每个套件包含一系列评测内核及预定义参数、计分规则及公式，输出评测过程和结果评分。

## 2.2 tpbcli run

### 2.2.1 基础格式

`tpbcli run` 的命令行格式如下所示，通过搭配 `kargs[_dim]`/`kenvs[_dim]`/`kmpiargs[_dim]` 选项，可以运行多个评测内核的评测，并为不同评测内核创建不同的参数组合，从而使用一条命令运行多个评测内核的多维度可变参数测试。
``` bash
tpbcli run <tpbench_options> <default_args> \
[--kernel <kernel_name> \
[--kargs/--kargs-dim <opts> | --kenvs/--kenvs-dim <opts> | --kmpiargs <opts> | --kmpiargs-dims <opts>]]
```
在上述命令格式中，所有尖括号“\<\>”选项均需要被实际使用的选项名称替换。

### 2.2.2 运行基础评测

**1）选择评测内核和内核参数**（`--kernel`, `--kargs`）

`--kernel` 选项定义一个即将被测试的评测内核，名为 `<kernel_name>`。随后出现的所有以 `--k*` 开头的选项代表内核选项，被应用于该评测内核，直至出现下一个 `--kernel` 或命令行结束。如果 `--kargs`、`--kenvs` 和 `--kmpiargs` 之前未指定 `--kernel`，则该设置的参数作为默认参数，传递给所有待运行内核。TPBench 将执行若干轮测试，直至完成所有 `--kernel` 定义的内核测试，或中途报错退出。一个 `--kargs` 可以设置多个参数，参数之间使用逗号隔开。若等号后使用逗号作为参数设置的一部分，则需要使用引号包裹，防止错误解析。

语法：`--kernel <kernel_name> --kargs <key1>=<value1>,<key2>=<value2>,<key3>="<v3>,<with>,<complex>,<section>",...`

对于一个 `--kernel` 定义，`--kargs`、`--kenvs` 和 `--kmpiargs` 可以出现多次，但是带有后缀 `_dim` 的选项只能出现一次。当同一参数名在一个 `--kernel` 定义后出现多次时，TPBench 将使用最后一次出现的值。相似地，如果 `--kernel <foo>` 后的 `--kargs` 定义与默认参数设置重复，那么 `<foo>` 将使用其作用域中最后一次出现的参数。例如，以下三条指令的作用是一样的。

```
$ tpbcli --kargs memsize=128,ntest=100 --kernel triad
$ tpbcli --kargs ntest=10 --kernel triad --kargs memsize=128,ntest=100
$ tpbcli --kernel triad \
         --kargs memsize=128 \
         --kargs ntest=100
```

需要注意，默认参数集或输入参数并不总是能被原样采用，主要有2个原因：1）参数处理由评测内核定义；2）评测内核不一定支持所有默认参数名。以总内存容量（`memsize=128`）为例，使用 `double` 精度时，triad 计算（`a_i=b_i+s*c_i`）会为每个数组开辟 5461 个 double 变量所需内存（128/3*1024/sizeof(double)，下取整），实际的总内存容量为 **131064 Bytes**，而不是 **131072 Bytes**。因此，在每轮测试结束后，评测内核实际使用的参数将被输出至终端，用户应当以此作为评测内核的实际输入参数。

示例1：运行 triad 内核，总内存容量 128KiB
```bash
$ tpbcli run --kargs memsize=128 -k triad
```

示例2：测试 2 轮 triad 内核，第 1 轮测试运行 100 个循环，总内存容量 128KiB；第 2 轮测试运行 100 个循环，总内存容量 256KiB
```bash
$ tpbcli run --kargs ntest=100 -k triad --kargs memsize=128 -k triad --kargs memsize=256
```

示例3：先后运行 triad、pchase 两个内核，每个内核的总内存容量 128KiB。triad 循环 100 次，pchase 循环 1000 次。
```bash
$ tpbcli run --kargs memsize=128,ntest=100 -k triad -k pchase --kargs=1000
```

使用 `--kernel -l` 可以列出目前可用的评测内核，使用 `--kernel <foo> --kargs -l` 可以列出 `<foo>` 内核支持的命令行输入参数。

### 2.2.3 运行可变参数评测

配置可变参数评测时，TPBench 按照预定义的取值序列逐个运行测试：每个取值都会执行一次内核，从而得到参数沿坐标轴变化时的结果（例如性能随某参数变化的曲线）。当指定一个评测内核 `--kernel <foo>` 时，在 `foo` 内核中可以通过 `--kargs` 配置的任一参数，均可以通过 `--kargs-dim` 配置为可变参数。若可变参数名与 `--kargs` 中配置的参数名重合，则 `--kargs` 中的同名参数将无效。

目前，`tpbcli run` 支持配置单一参数线性序列、显式列表和递推序列。若需要遍历多个维度，则可以嵌套多个参数序列。

**1）线性序列**

对于指定评测内核的一个参数，生成一个步长确定的连续序列。语法如下所示，对参数 `<parm_name>`，生成从 `st` 出发、步长为 `step` 的列表，参数值满足闭区间 `[st,en]`。

语法：`--kargs-dim <parm_name>=(st,en,step)`

示例：运行 `triad` 内核，使用 `double` 数据类型，每轮测试运行 100 次循环。配置内存总容量 `memsize` 为可变参数，该参数为线性序列，以 128KiB 为间隔，令 128KiB <= memsize <= 512KiB。TPBench 将运行 4 轮 `triad` 测试，分别将 `memsize` 参数设置为 128、256、384、512。

```
$ tpbcli --kernel triad --kargs ntest=100,dtype=double --kargs-dim memsize=(128,512,128)
```

**2）显式列表**

对于指定评测内核的一个参数，生成一个显式指定元素的集合，依次使用集合中的每个元素运行评测。语法如下所示，对参数 `<parm_name>`，生成集合，包含元素 `a`、`b`、`c`……

语法：`--kargs-dim <parm_name>=[a, b, c, ...]`

示例：运行 `triad` 内核，设置总内存容量 256KiB，每轮测试运行 100 次循环。配置数据类型 `dtype` 为可变参数，该参数为显式列表序列，包含 `double`、`float`、`iso-fp16`。TPBench 将轮流使用上述 3 种变量格式，共运行 3 轮 triad 测试。

```
$ tpbcli --kernel triad --kargs ntest=100,memsize=256 --kargs-dim dtype=[double,float,iso-fp16]
```

**3）递推序列**

对于指定评测内核的一个参数，生成一个递推参数序列。初值 `st`，使用计算 `<op>`，基于上一次测试的参数值 `@` 计算出新参数值。上述参数配置中，符号 `@` 表示递推变量；`op` 表示操作符，目前支持 add、sub、mul、div 和 pow；`st` 表示 `parm_name` 参数首次测试的值；`min`、`max` 和 `nlim` 分别表示递推结果的最小值、最大值和递推步数限制。当递推结果超出 `[min, max]` 区间，或递推步数高于 `nlim` 时，递推将终止。`nlim` 设置为 0 时，不限制递推步数。

语法：`--kargs-dim <parm_name>=<op>(@,x)(st,min,max,nlim)`

示例：运行 `triad` 内核，每轮执行 100 个循环，轮流将 `memsize` 设置为 `16`、`32`、`64`、`128`，共运行 4 轮测试。

```
$ tpbcli --kernel triad --kargs ntest=100 --kargs-dim memsize=mul(@,2)(16,16,128,0)
```

**4）嵌套序列**

嵌套多个可变参数，每个可变参数定义后，可以在大括号中定义另一个可变参数。在命令行解析中，最内层的参数列表将被优先计算。注意，目前不允许在不同嵌套层定义相同参数名。

语法：`--kargs-dim <dim>{<nested_dim1>{<nested_dim2>{...}}}`

示例：运行 `triad` 内核，每轮执行 100 个循环，轮流使用 `double`、`float` 和 `iso-fp16` 这三种数据格式。对于每种格式，轮流将 `memsize` 设置为 `16`、`32`、`64`、`128`，共运行 12 轮测试。

```
$ tpbcli --kernel triad --kargs ntest=100 --kargs-dim dtype=[double,float,iso-fp16]{memsize=mul(@,2)(16,16,128,0)}
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
$ tpbcli --kernel triad --kargs ntest=100,memsize=128 --kenvs OMP_NUM_THREADS=16,OMP_PLACES="{0:4}:4:4"
```


### 2.2.6 设置 MPI 运行参数
