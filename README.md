# TPBench - A High-Precision Throughputs Benchmark Tool

## 1 - Introduction

### 1.1 - License

TPBench - A High-Precision Throughputs Benchmark Tool (v0.3-beta)

Copyright (C) 2020 Key Liao (Liao Qiucheng)

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

### 1.2 - Introduction

TPBench is a high-precision micro benchmarking suite for basic instruction and computing kernels on HPC system. It aims at enabling the ability for users to measure the low-level performance in software stack from single-core to exascale comuting system. It's well documented, easy to use and customize. Use cases are included but not limited to:

* Measuring actual instruction latency and throughputs.
* Measuring hierarchical bandwidth and latency of storage subsystem.
* Micro benchmarking of basic scientific computing kernels.
* Customizing your own benchmarking tool.
* ...

TPBench currently supports benchmarks for both specific computing kernel and kernel groups, you can use switch between them using <-k> and <-g>. Supported kernel and groups are listed below.

* Group **io_ins**
    
    **AArch64**
    | Kernels   | Calculation             | IO Ops  | Arith Ops | IO Bytes/Test|      Notes      |
    | ---       |  ---                    | :---:   | :---:     | :---:        |    :---:        |
    | ldrx      | ldr Xn, [addr]          | L1S0WA0 | 0         |   8          |                 |
    | ldrq      | ldr Qn, [addr]          | L1S0WA0 | 0         |   16         |                 |
    | ldpx      | ldp Xn1, Xn2, [addr]    | L2S0WA0 | 0         |   16         |                 |
    | ldpq      | ldp Qn1, Qn2, [addr]    | L2S0WA0 | 0         |   32         |                 |
    | ldnpx     | ldnp Xn1, Xn2, [addr]   | L2S0WA0 | 0         |   16         |                 |
    | ldnpq     | ldnp Qn1, Qn2, [addr]   | L2S0WA0 | 0         |   32         |                 |
    | strx      | str Xn, [addr]          | L0S1WA1 | 0         |   16         |                 |
    | strq      | str Qn, [addr]          | L0S1WA1 | 0         |   32         |                 |
    | stpx      | stp Xn1, Xn2, [addr]    | L0S2WA2 | 0         |   32         |                 |
    | stpq      | stp Qn1, Qn2, [addr]    | L0S2WA2 | 0         |   64         |                 |
    | stnpx     | stnp Xn1, Xn2, [addr]   | L0S2WA0 | 0         |   16         |                 |
    | stnpq     | stnp Qn1, Qn2, [addr]   | L0S2WA0 | 0         |   32         |                 |

    **X86-64**

* Group **arith_ins**

    **AArch64**
    | Kernels   | Calculation             | IO Ops  | Arith Ops | Bytes/Test|      Notes      |
    | ---       |  ---                    | :---:   | :---:     | :---:     |    :---:        |
    | add| ldr Xn, [addr]          | L1S0WA0 | 0         |   8       |                 |
    | mul      | ldr Qn, [addr]          | L1S0WA0 | 0         |   16      |                 |
    | fmla      | ldp Xn1, Xn2, [addr]    | L2S0WA0 | 0         |   16      |                 |
    | sqrt      | ldp Qn1, Qn2, [addr]    | L2S0WA0 | 0         |   32      |                 |

    **X86-64**


* Group **level1_kern**
    | Kernels <-k> | Calculation | IO Ops | Arith Ops|
    | --- | ---     | ---         | ---             | ---|
    | Init   |a[i] = s |L0S1WA1 |0 |
    | Sum    |s += a[i] |L1S1WA0  |1 |
    | Copy   |a[i] = b[i] |L1S1WA1 | 0 |
    | Update |b[i] = b[i] *s | L1S1WA0 | 1 |
    | Triad  |b[i] = a[i] + s* c[i] |L2S1WA1 | 2 |
    | Daxpy  |a[i] = a[i] + s *b[i] |L2S1WA0 | 2 |
    | STriad |b[i] = a[i] + c[i]* d[i] |L3S1WA1  |  2 |
    | SDaxpy |d[i] = d[i] + b[i] * c[i] |L3S1WA0 | 2 |

* Group **level2_kern**

    **TBD**
* Group **level3_kern**

    **TBD**

TPBench is developed by Key Liao, Dr. James Lin in Center for High-Performance Computing, Shanghai Jiao Tong University. The projects provide cycle-level parallel timer and parallel bandwidth tests for Armv8 processors. The source is provide "AS-IS" and only tested on ThunderX2, KunPeng-920 and BCM2711.

### 1.4 - Acknowledgement

- This project was *initially* inspired by John McClpin's **STREAM Benchmark** and RRZE's **TheBandwidthBenchmark**.
- This project is currently developed and tested using computing facility in the Center of High-Performance Computing, Shanghai Jiao Tong University.
- Any kinds of support are welcome.

**(Tested on Xeon 6148, Thunder X2 and KP920, CentOS 7.4, GCC-9.2.0, OpenMPI-4.0.1)**



## 2 - Build and Run

### 2.1 - Quickstart

<!-- <code> -->
$ tar xf TPBench.tar.gz <br>
$ cd pmu && make && sudo insmod enable_pmu.ko && cd - <br>
$ make SETUP=GCC
</code>

### 2.2 - Specifications

- Tested on ThunderX2 and Hisilicon KunPeng 920.
- Detailed data in ./data is more useful than screen outputs.

### 2.3 - Build and run

  **Prequisites:**
  
  Tested with

- An MPI Library.(e.g. OpenMPI-4.0.1)
- GCC-9.2.0
- Root permission for loading cycle-level timer on AArch64.

 **Preparing**

 Loading kernel module to enable Armv8's PMU.

 <code>
 cd ./pmu <br/>
 make <br/>
 sudo insmod ./enable_pmu.ko <br/>
 </code>

 **Example serial run:**

 make clean && make -f Make.gcc && numactl -C 22 ./bench.x

 **Example parallel run:**

 make clean && make -f Make.mpi && mpirun --bind-to cpulist:ordered -np 8 ./bench_mpi.x

### 2.4 - Customization

Modifying timer with <code>rdtscp</code> to adjust for x86.

### 2.5 - TODO

Using virtual timer to sync instead of MPI_Barrier().

## 3 - FAQ

## 4 - Changelog

### Version 0.3

- Refactor whole project, reorganize source tree.
- Update README.md, add detail description and quickstart samples.
- Reorganize kernels into groups.
- Remove data dependencies between successive kernels.
- Add benchmark kernels for store, load and arithmetic instructions.
- Optimize timer.

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

Putting issues with resonable advices or reproducible problems.
