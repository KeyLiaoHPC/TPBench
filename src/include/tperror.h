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
 * @file tperror.h
 * @version 0.3
 * @brief Header for tpbench error handling
 * @author Key Liao (keyliaohpc@gmail.com, keyliao@sjtu.edu.cn)
 * @date 2024-01-26
 * =================================================================================
 */

// Error macro for translating error messages into tpbench error
#define __error_lt(evar, eno, err_name) if((evar) < (eno)) {    \
                                            return (err_name);  \
                                        }

#define __error_gt(evar, eno, err_name) if((evar) > (eno)) {    \
                                            return (err_name);  \
                                        }

#define __error_eq(evar, eno, err_name) if((evar) == (eno)) {   \
                                            return (err_name);  \
                                        }

#define __error_ne(evar, eno, err_name) if((evar) != (eno)) {   \
                                            return (err_name);  \
                                        }

#define __error_exit(evar) if((evar)) {exit(1);}

#define __error_fun(err, msg)   if((err)) { \
                                    err = tpprintf(err, 1, 1, msg);   \
                                    __error_exit(err);                  \
                                }
// ERROR CODE

#define NO_ERROR        0
// 1 - 50, tpbench general error
#define GRP_ARG_ERROR   1
#define KERN_ARG_ERROR  2
#define KERN_NE         3
#define GRP_NE          4
#define SYNTAX_ERROR    5
#define FILE_OPEN_FAIL  6
#define MALLOC_FAIL     7
#define ARGS_MISS       8
#define MKDIR_ERROR     9
#define RES_INIT_FAIL   10

// 51-100, MPI error
#define MPI_INIT_FAIL   51

// WARNING
#define VERIFY_FAIL     101
#define OVER_OPTMIZE    102
#define DEFAULT_DIR     103
#define CORE_NOT_BIND   104

#define DHLINE "================================================================================\n"
#define HLINE  "--------------------------------------------------------------------------------\n"
