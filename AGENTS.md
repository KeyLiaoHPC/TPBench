# TPBench Development Instructions

These instructions apply to all AI-assisted contributions to TPBench. Breaching these instructions can result in automatic banning.

## 1 AI Agent Guidelines

### 1.1 Basic version

The basic version supports single-core kernels and pthread-based mutlithread kernels.

#### 1.1.1 Build

``` bash
# Execute in the root folder of the workspace.
$ cmake -B build
$ cmake --build build --config Release
```

### 1.1.2 Running tests

Step 1: Run the ctest suite. If any tests fail, check the issues and fix.

```bash
# Execute in the root folder of the workspace.
$ cd build
$ ctest
```

Step 2: Run a single-core kernel. If the kernel does not finish normally and print "triad_bw_walltime" that larger than 1000 MB/s, check the issues and fix.

```bash
# Execute in the root folder of the workspace.
$ ./build/bin/tpbcli run --kernel stream --kargs total_memsize=524288,ntest=100
```