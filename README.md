# TPBench - Traverse Profiling as Benchmark

TPBench is a micro benchmarking tool. It acts as a framework for developing, running, and managing micro kernels for benchmarking your systems for multiple targets. TPBench integrates facilities for measuring and storing compiling options, environment variables, dependencies information, input arguments, output metrics, process variables, etc. With TPBench, you can create benchmark workloads, group different workloads in to a customizable benchmark suite with your own score rules, trace and analyze any data you want.

TPBench provides native-C frontends:

- `tpbcli run`: Evaluate the performance and health with common micro computing kernels (e.g. arithmetic instructions, the STREAM benchmark, stencils, AI operators, etc).
- `tpbcli benchmark`: Run predefined benchmark suites, calculate score and compare the performance from multiple dimensions you care.
- `tpbcli database`: Record and track the performance.

Refer to documentations in `docs/` for:

- Build and basic usages ([docs/USAGE.md](docs/USAGE.md)). 基础编译及使用方法：[USAGE_CN.md](docs/USAGE_CN.md).
- Cheatsheet/命令和选项速查表：[English](docs/cheatsheet.md)/[中文](docs/cheatsheet_CN.md)。
- Programming interfaces ([docs/API_Reference.md](docs/API_Reference.md)).
- Case studies and examples ([docs/EXAMPLES.md](docs/EXAMPLES.md)).

## Quickstart: 4-rank `stream_mpi` (512 MiB per core)

This walkthrough matches the supported workflow: **default TPBench build**, then add the MPI STREAM kernel with **`tpbcli kernel build`**, then run with **`mpirun`**. Example target: **56 MPI ranks**, **`--map-by core --bind-to core`**, **512 MiB per array per rank**.

### 1. Environment (optional)

If MPI (and related libraries) are not on your default `PATH` / `LD_LIBRARY_PATH`, set them before build and run. Adjust paths for your site:

```bash
export MPI=/path/to/your/mpi/          # Open MPI install root
export TPB_HOME=${HOME}/.tpbench  # where tpbcli + libtpbench.so live
export TPB_WORKSPACE=${TPB_HOME}    # rafdb workspace (logs, task records)
export PATH=${TPB_HOME}/bin:${MPI}/bin:${PATH}
export LD_LIBRARY_PATH=${TPB_HOME}/lib:${MPI}/lib:${LD_LIBRARY_PATH}
```

`TPB_HOME` is used to find `bin/`, `lib/`, and kernel sources for `tpbcli kernel build`. `TPB_WORKSPACE` is where run logs and rafdb records are stored (defaults to the same tree when unset at run time).

### 2. Build and install TPBench (default kernels)

From the TPBench source tree. Default **`TPB_KERNELS=default`** builds CPU kernels `stream`, `triad`, `scale`, `axpy`, `rtriad`, and `sum`. MPI kernels such as `stream_mpi` are **not** built here; add them in the next step.

Install prefix defaults to **`$HOME/.tpbench`**. To install elsewhere, set **`TPB_WORKSPACE`** (or **`CMAKE_INSTALL_PREFIX`**) **before** the first `cmake -B`:

```bash
git clone https://github.com/KeyLiaoHPC/TPBench.git
cd TPBench
cmake -B build
cmake --build build --config Release
cmake --install build
ls ${HOME}/.tpbench
# bin  etc  include  lib  rafdb  src
```

If a previous configure left **`TPB_USE_AVX512=ON`** in the cache but kernel objects fail with AVX-512 “target specific option mismatch”, reconfigure with the default (**OFF**):

```bash
cmake -B build -DTPB_USE_AVX512=OFF
cmake --build build --config Release
cmake --install build
```

### 3. Add the `stream_mpi` kernel

Build and install **`libtpbk_stream_mpi.so`** into **`$TPB_HOME/lib`** using the MPI wrapper compiler:

```bash
tpbcli kernel build --kernel stream_mpi --cc mpicc
# expect: kernel build: stream_mpi PASS
tpbcli kernel list | grep stream_mpi
```

This uses the registry under **`$TPB_HOME/src/kernels/`** and does not require reconfiguring the main TPBench tree with **`-DTPB_KERNELS=stream_mpi`**.

### 4. Run: 4 ranks, 512 MiB per core

**Parameter semantics (code):** for `stream_mpi`, **`stream_array_size`** is the **total number of double elements per array summed over all MPI ranks**, not the per-rank size. Each rank gets `agg_elems / nprocs` elements (remainder to the last rank).

For **512 MiB per array per rank** with **`double` (8 bytes)**:

```text
elements_per_rank = 512 * 1024 * 1024 / 8 = 67108864
stream_array_size = elements_per_rank * nprocs
                  = 67108864 * 4 = 268435456
```

Run (default **`ntest=10`**, **`twarm=500`** ms):

```bash
tpbcli run --kernel stream_mpi \
  --kargs stream_array_size=268435456 \
  --wrapper mpirun --wrapper-args '-np 4 --map-by core --bind-to core'
```

TPBench prints the resolved command, for example:

```text
Exec: ... mpirun -np 4 --map-by core --bind-to core \
  .../tpbcli-pli-launcher .../libtpbk_stream_mpi.so clock_gettime 10 268435456 500
```

### 5. What to expect (FOM)

Rank 0 prints aggregate STREAM results. **FOM bandwidth** uses global aggregate array bytes and the **minimum time across ranks** per iteration (excluding the first iteration), then reports best / avg / min / max. Example shape of output:

```text
Total Aggregate Array size = 3268435456 (elements)
   Memory per array per MPI rank = 512.0 MiB (= 0.5 GiB).
Function    Best Rate MB/s  Avg time     Min time     Max time
Copy:         190087.3     0.355584     0.316326     0.417862
Scale:        188713.0     0.348230     0.318630     0.374268
Add:          222108.9     0.436748     0.406081     0.483628
Triad:        215378.9     0.454098     0.418770     0.565483
Solution Validates: avg error less than 1.000000e-13 on all three arrays
```

Registered FOM outputs (rank 0 summary): **`FOM,BANDWIDTH::* Best Rate MB/s`** and **`FOM,TIME::*`** for Copy / Scale / Add / Triad. Values appear in the run log; per-rank `.tpbr` records store **`EVENT,TIME::*`** iteration timings.

Change **`ntest`** or **`twarm`** with **`--kargs`**, e.g. `--kargs ntest=20,twarm=1000,stream_array_size=268435456`.

### 6. Check results

Logs and task records are under **`${TPB_WORKSPACE}/rafdb/`** (default **`~/.tpbench/rafdb/`**).

```bash
# Recent batches
tpbcli db list

# Last run log (hostname in filename)
LOG_FILE=$(ls -t ~/.tpbench/rafdb/log/tpbrunlog_*.log | head -1)
tail -80 "$LOG_FILE"
```

**MPI runs:** one **task capsule** groups all rank records (shown as 1 task in **`db list`**).

```bash
# Capsule lists member rank TaskIDs
tpbcli db dump --task-id <CapsuleID>

# Per-rank timings (FOM summary is in rank-0 log output, not always in .tpbr dump)
tpbcli db dump --task-id <Rank0TaskID>
```

Resolve capsule ID from a batch:

```bash
tpbcli db dump --tbatch-id <TBatchID> | grep -A1 "Record Data"
```

More detail: [docs/USAGE.md](docs/USAGE.md) §2.3 (database) and §2.4.6 (`kernel build`).

## More examples

**Single-process STREAM** (kernel built by default install):

```bash
tpbcli run --kernel stream --kargs stream_array_size=134217728,ntest=20
```

**Sweep array sizes** with **`--kargs-dim`**:

```bash
tpbcli run --kernel stream --kargs ntest=20 \
  --kargs-dim 'stream_array_size=[1398101,22369621,134217728]'
```

**Rebuild an MPI kernel after source changes:**

```bash
tpbcli kernel build --kernel stream_mpi --cc mpicc
```

Do **not** use stale **`cmake --build ... --target tpb_build_kernel`** examples for adding kernels after install; use **`tpbcli kernel build`** as above. To include MPI kernels in the **main** tree configure step instead, see [docs/USAGE.md](docs/USAGE.md) and **`TPB_KERNELS=stream_mpi`** with **`TPB_MPI_PATH`**.

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
along with this program.  If not, see [https://www.gnu.org/licenses/](https://www.gnu.org/licenses/).
