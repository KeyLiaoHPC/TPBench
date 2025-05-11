/*
 * =================================================================================
 * TPBench - A high-precision throughputs benchmarking tool for scientific computing
 * 
 * Copyright (C) 2024 Key Liao (Liao Qiucheng)
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
 * tpgroups.h
 * Description: Kernel informations.
 * Author: Key Liao
 * Modified: May. 9th, 2024
 * Email: keyliaohpc@gmail.com
 * =================================================================================
 */
#include <stdint.h>
#include <stdarg.h>


int report_performance(uint64_t **ns, uint64_t **cy, uint64_t total_wall_time, int nskip, int ntest, int nepoch, int N, int Nr, int skip_comp, int skip_comm);
void log_step_info(uint64_t **ns, uint64_t **cy, char *kernel_name, int ntest, int nepoch, int N, int Nr, int skip_comp, int skip_comm);
