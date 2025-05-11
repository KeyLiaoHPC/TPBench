# TPBench - A Comprehensive HPC Benchmark Tool and Framework

## 1 - Introduction

TPBench is a benchmarking tool for scientific computing and HPC. It enables users to evaluate and observe the performance characteristics of HPC hardware system and computing application in one application. In addition to existed benchmark kernels, TPBench provides fundamental facilaties for benchmarking including high-precision timer, data factory, statistical analyzer, etc. TPBench aims at enabling you to integrate your own scientific kernel and bridging the performance gaps from micro architecture benchmark to top-level parallel application.

With TPBench, you're able to:
* Efficiently evaluating hardware performance of a computing system.
* Using consistent criterion for benchmarking in ASM, mini kernel, mini applications and real applications.
* Customizing your own benchmarking tool.

* ...

TPBench is developed by Key Liao in Center for High-Performance Computing, Shanghai Jiao Tong University. 
TPBench is now in developing and early tests, and it's proviede AS-IS without any guarantee or warranty. Contacting Key Liao (keyliao@sjtu.edu.cn) if you have further question or advices.

## 2 - Build and Run

### 2.1 - Quickstart
An quick example on running a double-precision STREAM benchmark with TPBench.

Selecting serial or mpi version using -DUSE_MPI. Selecting appropriate config using make SETUP=\<config>

On X86-64 platform (e.g. Xeon 6148, Xeon 6248):
<!-- <code> -->
$ git clone https://github.com/KeyLiaoHPC/TPBench <br>
$ cd TPBench<br>
$ make SETUP=gcc_x86 tpbench.x <br>
$ ./tpbench.x -L <br>
$ ./tpbench.x -n 50 -s 65536 -g d_stream

</code>

On Armv8-a (aarch64) platform (e.g. Marvel ThunderX2): 
<!-- <code> -->
$ tar xf TPBench.tar.gz <br>
$ cd TPBench <br>
$ cd pmu && make && sudo insmod enable_pmu.ko && cd - <br>
$ make SETUP=tx2
$ ./tpbench.x -n 50 -s 65536 -k d_axpy
</code>

### 2.2 - Build
**Prerequisites**
- C Compiler
- MPI Library
- `sudo` permission (only used once for enabling userspace Arm PMU)
  
**Makefile**

Use Make.<Arch> in setup/ directory for achitecture-specific options. 

**PMU kmod**

For Armv8 architecture, TPBench provide a simple PMU kernel module to let you activate the user-level access to Armv8's Performance Monitor Units. Use it by simply enter the pmu/ directory and make using system-default compiler (tested by gcc-4.8.5).

**Command Line**


  -d, --data_dir[=PATH]      Optional. Data directory. <br>
  -g, --group=group_list     Group list. (e.g. -g d_stream). <br>
  -k, --kernel=kernel_list   Kernel list.(e.g. -k d_init,d_sum). <br>
  -L, --list                 List all group and kernels then exit. <br>
  -n, --ntest=# of test      Overall number of tests. <br>
  -s, --nkib=kib_size        Memory usage for a single test array, in KiB. <br>
  -?, --help                 Give this help list <br>
      --usage                Give a short usage message <br>
  -V, --version              Print program version <br>

### 2.3 - Data and timer
All tests include cycle-level timer and nanosecond-level timer. For now, every test include a 1-second warmup. 
After warming up, the first 10 results will be skipped, you can change the number of skipped results by setting __NSKIP macro at src/include/tpdata.h.
If '-d' option is not set, results will be automatically saved in data/${hostname} folder in the place you start the program.
The syntax of output csv file is \<prefix>-r\<rank#>_c<core#>-\<postfix>.csv

## 3 - Benchmarking Methodology

TPBench provides two different timing area to classify your benchmarking target, groups and kernels. The **group** targets are programs including multiple loop epoch which you want to measure. And the **kernel** target is those simple comupting kernel which you only want to measure the overall time. 

9 kernels and 2 groups are now officially supoort by TPBench.
Customizing benchmarking is not fully supported yet, but it's not complicated to add your own benchmarking target into TPBench by refering existing target code. I'm still working on customization stuffs. If you want to evaluate some simple kernel like BLAS1-rot, stencil, etc, just simple wrap up
your code with timer in tptimer.h, and put your interface definition and information into tpgroups.h or tpkernels.h

## 4 - FAQ

## 5 - Changelog

### Version 0.71
- Add support for Arm SVE with any width in kernel `d_fmaldr` and `d_mulldr`

### Version 0.7
- Add support to Armv8 NEON in kernel `d_fmaldr`

### Version 0.61
- Fix the method of timing the total wall time of the GEMM kernels in groups' test.

### Version 0.6
- Add the total wall time output of the GEMM kernels in groups' test.
- Change the files structure.
  - Add `tplog.h` and `tplog.c`.
  - Move the `report_performance` function and `log_step_info` function into above two files.
- Change the kernel `d_gemmbcast` into `d_gemm_bcast` to unify the kernel names' format.

### Version 0.5
- Add `log_step_info` function in group kernels to support recording every rank's every step's datas into a csv file.

### Version 0.4
- Add the AVX-2 support of every kernels with AVX-512.

### Version 0.3
- Break all benchmarking target into group and kernels.
- A easy-to-use command line interface.
- Reset Armv8 cycler counter to avoid overflow.

### Version 0.21

- Decrease C to avoid overflow when N is huge.

### Version 0.2

- Enable x86 cycle timer.
- Automatic print final log into overall.
- All detail timing results are placed in ./data/${KIB_SIZE}.
- Bug fixes.

### Version 0.1

- Initial beta version.
- Enable AArch64 cycle timer.

## 5 - Make contributions

Put issues with resonable advices or reproducible problems.

## License

TPBench - A High-Precision Throughputs Benchmark Tool (v0.3-beta)

Copyright (C) 2024 Key Liao (Liao Qiucheng)

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

