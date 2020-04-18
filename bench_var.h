/*
 * =================================================================================
 * Arm-BwBench - A bandwidth benchmarking suite for Armv8
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
 * 
 * =================================================================================
 * bench_var.h
 * Description: Predefined macros for benchmark. 
 * Author: Key Liao
 * Modified: Apr. 14th, 2020
 * Email: keyliaohpc@gmail.com
 * =================================================================================
 */

#ifndef BENCH_VAR
#define BENCH_VAR

#define VER "0.2"

#include <stdint.h>

//==================
// Single array size in KiB, offset isn't counted
#ifndef KIB_SIZE
#define KIB_SIZE 32768
#endif
// Second of warm-up runs. If it's undefined, auto warming ups will be executed.
#ifndef TWARM
#define TWARM 1
#endif
// Number of tests.
#ifndef NTIMES
#define NTIMES 20
#endif
// Data type. Only double is supported now.
#ifndef TYPE
#define TYPE double
#endif

#define A 0.0002
#define B 0.11
#define C -0.003
#define D 0.23
#define S 0.002


// Make the scalar coefficient modifiable at compile time.
// The old value of 3.0 cause floating-point overflows after a relatively small
// number of iterations.  The new default of 0.42 allows over 2000 iterations for
// 32-bit IEEE arithmetic and over 18000 iterations for 64-bit IEEE arithmetic.
// The growth in the solution can be eliminated (almost) completely by setting 
// the scalar value to 0.41421445, but this also means that the error checking
// code no longer triggers an error if the code does not actually execute the
// correct number of iterations!
#ifndef SCALAR
#define SCALAR 0.42
#endif
// Alignment for a single array.
// #ifndef ALIGN
// #define ALIGN 0
// #endif

#define DHLINE "===================================================================\n"
#define HLINE "-------------------------------------------------------------------\n"

#ifndef MIN
#define MIN(x,y) ((x)<(y)?(x):(y))
#endif
#ifndef MAX
#define MAX(x,y) ((x)>(y)?(x):(y))
#endif
//=================
//
typedef struct{
    char *name;
    uint64_t nbyte;
    int ops;
} Kern_Type;

void init(TYPE *, TYPE, int, uint64_t *);
void sum(TYPE *, TYPE *, int, uint64_t *);
void copy(TYPE *, TYPE *, int, uint64_t *);
void update(TYPE *, TYPE, int, uint64_t *);
void triad(TYPE *, TYPE *, TYPE *, TYPE, int, uint64_t *);
void daxpy(TYPE *, TYPE *, TYPE, int, uint64_t *);
void striad(TYPE *, TYPE *, TYPE *, TYPE *, int, uint64_t *);
void sdaxpy(TYPE *, TYPE *, TYPE *, int, uint64_t *);
#endif 

