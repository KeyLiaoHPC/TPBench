# TPBench - Traverse Profiling as Benchmark

TPBench is a micro benchmarking tool. It acts as a framework for developing, running, and managing micro kernels for benchmarking your systems for multiple targets. TPBench integrates facilities for measuring and storing compiling options, environment variables, dependencies information, input arguments, output metrics, process variables, etc. With TPBench, you can create benchmark workloads, group different workloads in to a customizable benchmark suite with your own score rules, trace and analyze any data you want.

TPBench provides native-C frontends:
- `tpbcli run`: Evaluate the performance and health with common micro computing kernels (e.g. arithmetic instructions, the STREAM benchmark, stencils, AI operators, etc).
- `tpbcli benchmark`: Run predefined benchmark suites, calculate score and compare the performance from multiple dimensions you care.
- `tpbcli database`: Record and track the performance.

Refer to documentations in `docs/` for:
- Build and basic usages ([docs/USAGE.md](docs/USAGE.md)). 基础编译及使用方法：[USAGE_CN.md](docs/USAGE_CN.md).
- Programming interfaces ([docs/API_Reference.md](docs/API_Reference.md)).
- Case studies and examples ([docs/EXAMPLES.md](docs/EXAMPLES.md)).

## 1. Get Started: Running a STREAM Benchmark

### 1.1. Compiling TPBench

Build from the TPBench source tree, then install to `~/tpbench`:

```bash
git clone https://github.com/KeyLiaoHPC/TPBench.git
export TPB_WORKSPACE=~/tpbench
cd TPBench
cmake -B build
cmake --build build --config Release
cmake --install build 
ls ~/tpbench
bin  etc  include  lib  rawdb
```

For running `tpbcli` linking `libtpbench.so`, set the library search path. You can also set $PATH for convenient:

```bash
export PATH=${HOME}/tpbench/bin:${PATH}
export LD_LIBRARY_PATH=$HOME/tpbench/lib:${LD_LIBRARY_PATH}
```

### 1.2. Running the STREAM benchmark

Run one STREAM test with `ntest=20` and `total_memsize=3145728`:

```bash
tpbcli run --kernel stream --kargs total_memsize=3145728,ntest=20
...
Result quantiles: Q0.05=1.1308E5, Q0.25=1.1769E5, Q0.50=1.3686E5, Q0.75=1.3734E5, Q0.95=1.3777E5
Metrics: triad_bw_walltime
Units: MB/s
Result mean: 1.3113E5
Result quantiles: Q0.05=9.9613E4, Q0.25=1.1762E5, Q0.50=1.4419E5, Q0.75=1.4967E5, Q0.95=1.5102E5
===
2026-03-24 15:16:54 [NOTE] Kernel stream finished successfully.
2026-03-24 15:16:54 [NOTE] Auto-record: task recorded, TaskID=cdad43d7505ec25d814f26b2d4b89d379cba9af8
2026-03-24 15:16:54 [NOTE] TPBench exit.
2026-03-24 15:16:54 [NOTE] Auto-record: batch ended, 1 tasks recorded.

```

You should see `TPBench workspace: .../tpbench`, kernel parameters, timing and bandwidth metrics, `Solution Validates`, and a final success note.

To sweep three memory sizes 32 MiB, 512 MiB, and 3 GiB, set `--kargs-dim`:

```bash
tpbcli run --kernel stream --kargs ntest=20 --kargs-dim 'total_memsize=[32768,524288,3145728]'
```

This command expands to three runs and records three tasks in one batch.

### 1.3. Checking results

Log files and records of arguments, task results are automatically saved in `${TPB_WORKSPACE}/rawdb`. So you don't have to keep terminal outputs. Here the "node01" is the hostname of your system, you need to adjust to align with your own system:

```bash
# Here the "node01" is the hostname of your system, you need to adjust to align with your own system.
tree ~/tpbench/rawdb
/home/hpckey/tpbench/rawdb/
├── kernel
│   ├── ad57c7c94b52b18c042ed7036b9818391968c8b5.tpbr
│   └── kernel.tpbe
├── log
│   ├── tpbrunlog_20260324T151817_node01.log
│   └── tpbrunlog_20260324T151959_node01.log
├── task
│   ├── 1b809109eee590f5d865f245fa0ebcd86f6ce159.tpbr
│   ├── 4c48e958bcb93c21609bbb5e4d509943212d197f.tpbr
│   ├── 7c25f0b333842ddc316d8e7d7377aaf969ee9eba.tpbr
│   ├── cdad43d7505ec25d814f26b2d4b89d379cba9af8.tpbr
│   └── task.tpbe
└── task_batch
    ├── 28d62c15ee2941aceb75b66f26d3d03641233520.tpbr
    ├── ca394f1f07e576ff0a281cf3af3029eb38b8cba3.tpbr
    └── task_batch.tpbe
tail ~/tpbench/rawdb/log/tpbrunlog_20260324T151817_node01.log
Result quantiles: Q0.05=1.1609E5, Q0.25=1.1766E5, Q0.50=1.1816E5, Q0.75=1.1871E5, Q0.95=1.3181E5
Metrics: triad_bw_walltime
Units: MB/s
Result mean: 1.1697E5
Result quantiles: Q0.05=9.9532E4, Q0.25=1.1730E5, Q0.50=1.1825E5, Q0.75=1.1879E5, Q0.95=1.3678E5
===
2026-03-24 15:18:22 [NOTE] Kernel stream finished successfully.
2026-03-24 15:18:22 [NOTE] Auto-record: task recorded, TaskID=4c48e958bcb93c21609bbb5e4d509943212d197f
2026-03-24 15:18:22 [NOTE] TPBench exit.
2026-03-24 15:18:22 [NOTE] Auto-record: batch ended, 3 tasks recorded.
```

Use `tpbcli database` to check detailed record, you can find the TasiID above is `4c48e958bcb93c21609bbb5e4d509943212d197f` :
```bash
# Dumping raw data of tpbr
tpbcli d dump --id 4c48
# You can also check all recorded task's ID by dumping the entry file (.tpbe)
tpbcli d dump --entry task
```

### 1.4. Building and running the parallel STREAM benchmark

By default, TPBench only builds a STREAM benchmark. You can compile any kernels into your TPBench installation by setting `--target tpb_build_kernels`, set `-DTPB_KERNELS` or `-DTPB_KERNEL_TAGS` to select test kernels and set `-DTPB_KERNEL_CFLAGS`/`-DTPB_KERNEL_CXXFLAGS`/`TPB_KERNEL_FFLAGS` to configure compiler options. After building, use custom build target `--tpb_install_kernel` to install the new kernels to your `$TPB_WORKSPACE`.

This example shows how to add a STREAM-MPI kernel and run it with 2 MPI processes and 2 OpenMP threads per MPI rank. Please set the `-DTPB_MPI_PATH` to your actual MPI library path. 
``` bash
# If TPBench has not been built, in TPBench root directory
cmake -B build -DTPB_KERNELS=stream_mpi -DTPB_ENABLE_OPENMP=ON -DTPB_MPI_PATH=/path/to/mpi/library
# Or, if you want to add one more kernel when after installing TPBench
cmake --build build --target tpb_build_kernel  \
-DTPB_KERNELS=stream_mpi -DTPB_ENABLE_OPENMP=ON -DTPB_MPI_PATH=/path/to/mpi/library
# Install and run the kernel. Here you can use 'r' for 'run'.
cmake --build build --target tpb_install_kernel
tpbcli r --kernel stream --kargs ntest=10,stream_array_size=67108864 --kenvs 'OMP_NUM_THREADS=2' --kmpiargs '-np 2'
```


## License

TPBench

Copyright (C) 2024 - 2026 Key Liao (Liao Qiucheng), Center for High-Performance Computing, Shanghai Jiao Tong University.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.

