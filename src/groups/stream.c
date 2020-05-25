/**
  * =================================================================================
  * TPBench - A high-precision throughputs benchmarking tool for HPC
  * 
  * Copyright (C) 2020 Key Liao (Liao Qiucheng)
  * 
  * This program is free software: you can redistribute it and/or modify it under the
  *  terms of the GNU General Public License as published by the Free Software 
  * Foundation, either version 3 of the License, or (at your option) any later 
  * version.
  * 
  * This program is distributed in the hope that it will be useful, but WITHOUT ANY 
  * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A 
  * PARTICULAR PURPOSE. See the GNU General Public License for more details.
  * You should have received a copy of the GNU General Public License along with 
  * this program. If not, see https://www.gnu.org/licenses/.
  * =================================================================================
  * 
  * @file stream.c
  * @version 0.3
  * @brief   A TPBench-port John McCalpin's STREAM benchmark.
  * @author Key Liao (keyliaohpc@gmail.com, keyliao@sjtu.edu.cn)
  * @date 2020-05-22
  */

#define _GNU_SOURCE
#include <stdint.h>

int
d_stream(int ntest, int nepoch, uint64_t **ns, uint64_t **cy, uint64_t kib) {
    return 0;
}