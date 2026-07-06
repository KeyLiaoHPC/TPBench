# 配置、编译和测试

TPBench 配置、编译和测试。

## 必选项


| 选项                          | 用途                                                            |
| --------------------------- | ------------------------------------------------------------- |
| `cmake -B <build-dir>`      | 配置与生成TPBench目标代码。要求CMake≥3.16。                                |
| `cmake --build <build-dir>` | 编译默认目标（`tpbcli`、`libtpbench.so`、选中的 `libtpbk_*.so`，以及启用时的测试）。 |


### `ctest`（启用测试时）


| 选项                                         | 用途                                                             |
| ------------------------------------------ | -------------------------------------------------------------- |
| `cd <build-dir> && ctest`                  | 运行 CTest 套件。`BUILD_TESTING=ON`（默认）时可用。                         |
| `cmake --build <build-dir> --target check` | 构建测试依赖并执行 `ctest --output-on-failure`（`tests/CMakeLists.txt`）。 |


每个 CTest 用例会先经 `tests/RunBuiltTest.cmake` 构建对应 target，再运行测试二进制。

## 可选项

### 构建命令


| 选项                                            | 用途                                                                                                              |
| --------------------------------------------- | --------------------------------------------------------------------------------------------------------------- |
| `cmake --build <dir> --config Release`        | 多配置生成器下指定构建类型（会传给测试构建的 `CTEST_CONFIGURATION_TYPE`）。                                                             |
| `cmake --build <dir> --target <name>`         | 编译单个目标或组件。常用：`tpbcli`、`tpbench`、`tpbk_<kern>`、`tpb_cmake_help`、`tpb_build_kernel`、`tpb_install_kernel`、`check`。 |
| `cmake --install <dir> [--prefix PATH]`       | 安装核心库与 CLI；kernel `.so` 用 `--component tpbench_kernels`。`--prefix` 覆盖已配置的 `CMAKE_INSTALL_PREFIX`。               |
| `cmake --build <dir> --target tpb_cmake_help` | 打印全部 `TPB_*` 缓存项与 kernel 目录（无需编译）。                                                                              |


### Kernel 选择（`-D`）


| 选项                                | 默认值            | 用途                                                                          |
| --------------------------------- | -------------- | --------------------------------------------------------------------------- |
| `-DTPB_KERNELS=<list>`            | `default`      | 逗号分隔的 kernel 名，或 `all` / `default`。选择逻辑见 `cmake/TPBenchKernelSelect.cmake`。 |
| `-DTPB_KERNEL_TAGS=<list>`        | *（空）*          | 逗号分隔标签；任一标签匹配即选中（与 `TPB_KERNELS` 取并集）。                                      |
| `-DTPB_KERNEL_<NAME>_TAGS=<list>` | *（按 registry）* | 覆盖单个 kernel 的默认标签（`<NAME>` 大写，如 `TPB_KERNEL_STREAM_TAGS`）。                  |


`**TPB_KERNELS` 候选：** `default`、`all`，或 `cmake/TPBenchKernelRegistry.cmake` 中的名称（如 `stream`、`triad`、`scale`、`axpy`、`rtriad`、`sum`、`staxpy`、`striad`、`stream_mpi`、`roofline_rocm`）。

**常用标签候选：** `default`、`bandwidth`、`stanza`、`mpi`、`gpu`、`roofline`、`rocm`。

### 编译器、编译选项与并行（`-D`）


| 选项                                           | 默认值           | 用途                                                                           |
| -------------------------------------------- | ------------- | ---------------------------------------------------------------------------- |
| `-DCMAKE_C_COMPILER=<path>`                  | *（自动）*        | 宿主 C 编译器，用于 corelib、CLI、CPU kernel（标准 CMake）。                                |
| `-DCMAKE_CXX_COMPILER=<path>`                | *（自动）*        | C++ 编译器，用于 ROCm kernel 入口源（标准 CMake）。                                        |
| `-DCMAKE_BUILD_TYPE=Release|Debug|…`         | *（依生成器）*      | 全局构建类型；`TPB_RECORD_KERNEL_COMPILE_HISTORY=ON` 时会写入 kernel 编译元数据。             |
| `-DCMAKE_C_FLAGS="..."`                      | *（空）*         | 全局 C 编译选项；post-build 写入 `compilation.c_flags`。                               |
| `-DTPB_KERNEL_CFLAGS="..."`                  | *（空 → `-O2`）* | 非空时**替换** CPU kernel 的 C 编译选项（`src/kernels/CMakeLists.txt`）。                 |
| `-DTPB_KERNEL_CXXFLAGS="..."`                | *（空 → `-O2`）* | 非空时**替换** ROCm/HIP kernel 编译选项（`cmake/TPBenchGpuKernelsRocm.cmake`）。         |
| `-DTPB_KERNEL_FFLAGS="..."`                  | *（空 → `-O2`）* | 预留给未来 Fortran kernel。                                                        |
| `-DTPB_ENABLE_OPENMP=ON`                     | `OFF`         | 为已选中的测试内核添加 OpenMP 编译/链接。                                                    |
| `-DTPB_USE_AVX512=ON`                        | `OFF`         | 全工程定义 `TPB_USE_AVX512`。                                                      |
| `-DTPB_USE_AVX2=ON`                          | `OFF`         | 定义 `TPB_USE_AVX2`。                                                           |
| `-DTPB_USE_KP_SVE=ON`                        | `OFF`         | 定义 `TPB_USE_KP_SVE`。                                                         |
| `-DTPB_SHOW_DEBUG=ON`                        | `OFF`         | 在测试内核上定义 `TPB_K_DEBUG=1`。                                                    |
| `-DTPB_RECORD_KERNEL_COMPILE_HISTORY=ON|OFF` | `ON`          | 构建后调用 `tpbcli kernel set`，记录 `variation` / `compilation` / `dependency` 元数据。 |


### 路径与安装布局（`-D`）


| 选项                       | 默认值                   | 用途                                                                                |
| ------------------------ | --------------------- | --------------------------------------------------------------------------------- |
| `-DTPB_WORKSPACE=<path>` | `$ENV{TPB_WORKSPACE}` | 工作区根；当安装前缀仍为 CMake 默认值时，用作 `CMAKE_INSTALL_PREFIX`。                                |
| `-DTPB_HOME=<path>`       | `<build-dir>`         | 写入 `libtpbench.so`，供 dynloader 发现 kernel/launcher（`src/corelib/tpb-dynloader.c`）。 |
| `-DTPB_MPI_PATH=<path>`  | *（空）*                 | 选中 MPI kernel 时的 MPI 安装根（仅链接到 kernel .so，libtpbench 不链接 MPI）；空则自动探测。                                                  |
| `-DTPB_ROCM_PATH=<path>` | *（空）*                 | 选中带 `rocm` 标签的 GPU kernel 时的 ROCm 根；空则自动探测（可回退 `ROCM_PATH` 环境变量）。                 |
| `-DBUILD_TESTING=OFF`    | `ON`                  | 不构建测试、不调用 `enable_testing()`。                                                     |


## 环境变量

在 **CMake 配置/构建阶段** 生效的变量（除非注明，否则不是运行时 `tpbcli` 专用）：


| 变量              | 用途                                                                                                                      |
| --------------- | ----------------------------------------------------------------------------------------------------------------------- |
| `TPB_WORKSPACE` | 配置时若已设置，会种子 `-DTPB_WORKSPACE` 缓存及默认安装前缀。`cmake --build` 时若已设置，会传给 post-build 的 `tpbcli kernel set` / `backup-inactive`。 |
| `ROCM_PATH`     | `TPB_ROCM_PATH` 为空且不存在 `/opt/rocm` 时的 ROCm 根目录回退（根 `CMakeLists.txt`）。                                                   |
| `DESTDIR`       | `cmake --install` 时的 staging 前缀（根目录 `install(CODE …)`）。                                                                 |


---

# tpbcli

TPBench的命令行前端，负责运行和管理测试内核，管理测试记录。命令行格式：

```
tpbcli [--workspace PATH] <run/r>|<kernel/k>|<databases/db>|<benchmark/b>|<help/h> [options]
```

工作区解析顺序：`--workspace` → `$TPB_WORKSPACE` → `$HOME/.tpbench`（`src/corelib/tpb_corelib_state.c`）。

## tpbcli run / r

### 必选项


| 选项                              | 用途                                                       |
| ------------------------------- | -------------------------------------------------------- |
| `--kernel <name>` / `-k <name>` | 要运行的 kernel；标记为 `TPBCLI_ARGF_MANDATORY`。可重复以创建多个 handle。 |


### 可选项


| 选项                        | 用途                                                                 |
| ------------------------- | ------------------------------------------------------------------ |
| `--kargs <key=val,...>`   | 当前 handle 的运行时 kernel 参数（逗号分隔的 `key=value`）。                       |
| `--kargs-dim <spec>`      | 对 kernel 参数做维度扫描；多个 flag 做笛卡尔积（最多 16 个）。                           |
| `--kenvs <KEY=VAL,...>`   | 传给 kernel 进程的 per-handle 环境变量。                                     |
| `--kenvs-dim <spec>`      | 对环境变量做维度扫描。                                                        |
| `--wrapper <app>` | 启动 PLI wrapper 链（按顺序链接；第一个 `--kernel` 之前为全局，之后为当前 kernel 局部）。 |
| `--wrapper-args '<args>'` | 追加到最近一个 `--wrapper` 的参数（引号字符串）。 |
| `-og` / `--override-global` | 当前 kernel 忽略全局 wrapper 链（保留局部 wrapper）。 |
| `--timer <name>`          | 计时后端；预设默认 `clock_gettime`。候选：`clock_gettime`；`tsc_asym`（仅 x86_64）。 |
| `--outargs <key=val,...>` | 输出格式：`unit_cast=<int>`、`sigbit_trim=<int>`（默认 0 和 5）。              |
| `--dry-run` / `-d`        | 校验参数并打印命令，不实际执行 kernel。                                            |
| `--help` / `-h`           | 子命令或 per-kernel 帮助（`--kernel <name> --help` 列出参数/输出）。              |


**示例**

```bash
./build/bin/tpbcli run --kernel stream --kargs stream_array_size=524288,ntest=100
./build/bin/tpbcli r -k stream_mpi --kargs ntest=100 --wrapper mpirun --wrapper-args '-np 4'
```

### 环境变量


| 变量                    | 用途                                                          |
| --------------------- | ----------------------------------------------------------- |
| `TPB_WORKSPACE`       | 未指定 `--workspace` 时的 RAFDB 工作区根。                            |
| `TPB_TBATCH_ID`       | PLI 子进程录制用的 tbatch ID（`src/corelib/tpb-autorecord.c`）。      |
| `TPB_HANDLE_INDEX`    | PLI 子进程 capsule 链接用的 handle 索引。                             |
| `TPB_KERNEL_ID`       | PLI 子进程录制用的 Kernel ID 覆盖。                                   |
| `TPB_LOG_FILE`        | PLI 子进程追加写入的日志绝对路径（driver fork/exec 前设置；每次 `tpbcli` 启动时清除）。 |
| `TPBENCH_TIMER`       | driver 注入 PLI 子进程的计时器名称。                                    |
| `TPBENCH_UNIT_CAST`   | 传给子进程的单位转换标志（来自 `--outargs` 或环境变量）。                         |
| `TPBENCH_SIGBIT_TRIM` | 输出有效位截断（来自 `--outargs` 或环境变量）。                              |
| `OMP_NUM_THREADS`     | *（外部）* OpenMP kernel 的线程数；也可通过 `--kenvs` 设置。                |


## tpbcli kernel / k

子命令：`list`/`ls`、`get`、`set`、`init`、`build`、`backup-inactive`。**`list`** 扫描已安装 `.so` 及 **`$TPB_HOME/src/kernels/kernel_list.cmake.in`**，输出 **Kernel / KernelID / Tags / Description**（未编译行 KernelID 为 **`N/A`**）。

### `kernel build`

#### 必选（二选一）


| 选项 | 用途 |
| ---- | ---- |
| `--kernel <names>` | 逗号分隔内核名；可用 `'...'` 或 `"..."` 包裹。 |
| `--kernel-tag <tags>` | 逗号分隔标签；与 **`--kernel`** 互斥。 |


#### 可选


| 选项 | 用途 |
| ---- | ---- |
| `--dir <path>` | 默认 **`TPB_HOME`**；默认时按 registry 解析 **`$TPB_HOME/src/kernels/<PATH>`**。 |
| `--ldflags <flags>` | 链接选项 → **`TPB_KERNEL_LDFLAGS`** / **`compilation.kernel_ldflags`**。 |
| `--tpb-home`、`-D`、编译器选项 | 同英文 cheatsheet。 |


Registry：**`src/kernels/kernel_list.cmake.in`**（`NAME|TAGS|PATH`）。

### `kernel get`

#### 必选项


| 选项                | 用途                        |
| ----------------- | ------------------------- |
| `--kernel <name>` | 要在 RAFDB 中查询的逻辑 kernel 名。 |


#### 可选项


| 项                  | 用途                                              |
| ------------------ | ----------------------------------------------- |
| `-v` / `--verbose` | 列出**全部**变体（从新到旧）。默认：最新 active 记录，否则最新 inactive。 |


只读：不扫描 `.so`、不触发注册（`src/tpbcli-kernel-get.c`）。

### `kernel set`

#### 必选项


| 项                                  | 用途                                                                               |
| ---------------------------------- | -------------------------------------------------------------------------------- |
| `--kernel <name>`                  | 内核名称，匹配至active标记为1的内核记录。 kernel。                                               |
| `--key <section.subkey> '<value>'` | 要写入的元数据键（可重复，最多 32 对）。点分形式，如 `compilation.kernel_cflags`、`variation.build_type`。 |


必要时注册 kernel，然后更新 RAFDB 中的 `variation` / `compilation` / `dependency` 头。

### `kernel backup-inactive`

#### 必选项


| 项                 | 用途                                                                                                         |
| ----------------- | ---------------------------------------------------------------------------------------------------------- |
| `--kernel <name>` | 将 `lib/libtpbk_<name>.so` 移至 `lib/inactive/libkernel_<name>_<kernel_id>.so_bak`，并将对应 KernelID 标为 inactive。 |


### 环境变量


| 变量                          | 用途                                                     |
| --------------------------- | ------------------------------------------------------ |
| `TPB_WORKSPACE`             | RAFDB 读写的目标工作区。                                        |
| `TPB_K_OVERRIDE=1|true|yes` | 允许覆盖已存在的 KernelID 记录（`kernel set`、`kernel list`、动态加载）。 |


## tpbcli database / db

rafdb操作前端，子命令：`list`/`ls`、`dump`（`src/tpbcli-database.c`）。

### `database list` / `database ls`

```
tpbcli db list [-dT|-dt|-dk | --domain <tbatch|task|kernel>] [-n <N> | -N <N>]
```

默认：**`-dT`**（tbatch）与 **`-n 20`**（最近 20 条）。域选项与 `--domain` 互斥；**`-n`** 与 **`-N`** 互斥。

| 项 | 用途 |
| --- | --- |
| `list` / `ls` | 子命令。 |
| `-dT` | tbatch 域（默认）。 |
| `-dt` | task 入口点（`derive_to==0`）。 |
| `-dk` | kernel 域。 |
| `--domain <name>` | `tbatch`、`task` 或 `kernel`。 |
| `-n <N>` | 最近 N 条。 |
| `-N <N>` | 最旧 N 条。 |
| `--help` / `-h` | 子命令帮助。 |

**tbatch 列：** Start Time (UTC)、Type、Duration(s)、NTask、NKernel、NScore、TBatch ID（6 位 hex + `*`）。

**task 列：** Start Time (UTC)、Kernel、Exit、Duration(s)、Handle、Task ID、TBatch ID。

**kernel 列：** Kernel Name、Active、NParm、NMetric、Build Time (UTC)、Kernel ID。

表格使用 `tpblog_printf_c`（`src/tpbcli/database/tpbcli-database-ls.c`）。


### `database dump`

#### 必选项

**恰好一个**选择器（互斥）：


| 项                   | 用途                                          |
| ------------------- | ------------------------------------------- |
| `--id <hex>`        | 全局 SHA-1 搜索（4–40 位十六进制），跨域查找。               |
| `--tbatch-id <hex>` | 按 ID 导出 tbatch 记录。                          |
| `--kernel-id <hex>` | 按 ID 导出 kernel 记录。                          |
| `--task-id <hex>`   | 按 ID 导出 task 记录。                            |
| `--score-id <hex>`  | 接受该参数，但 score 域尚未实现（打印提示后以 0 退出）。           |
| `--file <path>`     | 导出 `.tpbr`/`.tpbe` 文件（工作区内 basename 或完整路径）。 |
| `--entry <name>`    | 导出域索引 CSV。候选：`task_batch`、`kernel`、`task`。  |


#### 可选项


| 项               | 用途         |
| --------------- | ---------- |
| `--help` / `-h` | dump 选项帮助。 |


### 环境变量


| 变量              | 用途                        |
| --------------- | ------------------------- |
| `TPB_WORKSPACE` | 记录查找用的工作区根（与其他子命令相同解析规则）。 |


**示例**

```bash
./build/bin/tpbcli db list
./build/bin/tpbcli database dump --entry kernel
./build/bin/tpbcli database dump --tbatch-id abcdef0123456789abcd
```

