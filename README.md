# TPBench - A Cycle-Level Throughputs Benchmark Tool
## 1 - Introduction
### 1.1 - License
TPBench - A Cycle-Level Throuputs Benchmark Tool (v0.1-beta)

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
TPBench is developed by Key Liao, Dr. James Lin in Center for High-Performance Computing, Shanghai Jiao Tong University. The projects provide cycle-level parallel timer and parallel bandwidth tests for Armv8 processors. The source is provide "AS-IS" and only tested on ThunderX2, KunPeng-920 and BCM2711.


| Kernels | Calculation | Data Operations | Ops|
| ---     | ---         | ---             | ---| 
| Init   |a[i] = s |1 Load, WA |0 |
| Sum    |s += a[i] |1 Load + 1 Store  |1 |
| Copy   |a[i] = b[i] |1 Load + 1 Store + WA | 0 |
| Update |b[i] = b[i] * s |1Load + 1 Store | 1 |
| Triad  |b[i] = a[i] + s * c[i] |2 Load + 1 Store + WA | 2 |
| Daxpy  |a[i] = a[i] + s * b[i] |2 Load + 1 Store | 2 |
| STriad |b[i] = a[i] + c[i] * d[i] |3 Load + 1 Store + WA  |  2 |
| SDaxpy |d[i] = d[i] + b[i] * c[i] |3 Load + 1 Store | 2 |

### 1.3 - Acknowledgement
 - This project is highly inspired by John McClpin's **STREAM Benchmark** and RRZE's **TheBandwidthBenchmark**.
 - This project is developed with computing facility in the Center of High-Performance Computing, Shanghai Jiao Tong University.
## 2 - Build and Run
### 2.1 - Specifications
 - Tested on ThunderX2 and Hisilicon KunPeng 920. 
 - Detailed data in ./data is more useful than screen outputs.
### 2.2 - Build and run
  **Prequisites:**
  
  Tested with
 - An MPI Library.(e.g. OpenMPI-4.0.1)
 - GCC-9.2.0
 - Root Autority(for loading PMU kernel module)
 
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
### 2.3 - Customization
Modifying timer with <code>rdtscp</code> to adjust for x86.
### 2.4 - TODO
Using virtual timer to sync instead of MPI_Barrier().
## 3 - FAQ

## 4 - Changelog

### Version 0.21
 - Decrease C to avoid overflow when N is huge.

### Version 0.2
 - Enable x86 cycle timer.
 - Automatic print final log into overall.csv
 - All detail timing results are placed in ./data/${KIB_SIZE}.
 - Bug fixes.

### Version 0.1
 - Initial beta version.
 - Enable AArch64 cycle timer.



## 5 - Make contributions
Putting issues with resonable advices or reproducible problems.

