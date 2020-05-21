/*
 * =================================================================================
 * TPBench - A high-precision throughputs benchmarking tool for scientific computing
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
 * error.h
 * Description: Headers for error handling.
 * Author: Key Liao
 * Modified: May. 15th, 2020
 * Email: keyliaohpc@gmail.com
 * =================================================================================
 */

// ERROR CODE
#define NO_ERROR        0
#define GRP_NOT_MATCH   1
#define KERN_NOT_MATCH  2
#define FILE_OPEN_FAIL  3
#define MALLOC_FAIL     4

// WARNING
#define VERIFY_FAIL     100
#define OVER_OPTMIZE    101