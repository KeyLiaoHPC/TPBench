/**
 * @file tpb_io.h
 * @brief Header for handling input/output of tpbench.
 */
#define _GNU_SOURCE

#include <stdint.h>
#include <limits.h>
#include "tpb-types.h"
#include "tpmpi.h"

#define TPBM_HELP_DOC_TOTAL \
    "\n" \
    "===========================================================================" \
    "Usage: tpbench <action> <option>\n" \
    "\n" \
    "Actions: run, list, help\n" \
    "Options and explanation for each action:\n" \
    "---------------------------------------------------------------------------" \
    "  run               Run one or more benchmark kernels.\n" \
    "      -kargs <key>=<value>\n" \
    "                    Default kernel run-time arguments.\n" \
    "                    Use 'help kargs' for supported keys and values.\n" \
    "      -memsize <kib_size>\n" \
    "                    Memory usage for a single test array, in KiB.\n" \
    "      -k <kernel1>:[[kargs0_0]:...:[kargs0_n]],<kernel2>:...\n" \
    "                    Kernel list separated by comma. Each kernel can have\n" \
    "                    multiple kargs separated by colon. (e.g.\n" \
    "                    d_init:memsize=1024,triad:fp=fp64:memsize=1024)\n" \
    "                    Use 'help <kernel>' for available options of a kernel.\n" \
    "      -d, <PATH>\n  Data directory. Default: ./data\n" \
    "      -t, --timer <timer_name>\n" \
    "                    Timer name. Supported: clock_gettime (default),\n" \
    "                    tsc_asym.\n" \
    "      -h, --help    Print this help message and exit.\n" \
    "---------------------------------------------------------------------------" \
    "  benchmark         Run predefined benchmark suites.\n" \
    "---------------------------------------------------------------------------" \
    "  list              List kernels, parameters, implemented routines, etc.\n" \
    "                    of an object then exit.\n" \
    "---------------------------------------------------------------------------" \
    "  help              Print help message for an object and exit.\n" \
    "===========================================================================" \
    "\n"


/**
* @brief 
*/
void tpb_list();

/**
* @brief 
* @param dirpath 
* @return int 
*/
int tpb_mkdir(char *dirpath);

/**
* @brief TPBench format stdout module. Return error level according to error number. 
*        Print message if tpmpi_info.myrank == 0. Output syntax:
*        YYYY-mm-dd HH:MM:SS [TAG ] *msg
* @param err   Error number described in tperror.h
* @param ts    Set to 0 to ignore timestamp.
* @param tag   Set to 0 to ignore error tag.
* @param msg   char *msg, same syntax as printf.
* @param ...   varlist, same syntax as printf.
*/
void tpb_printf(int err, int ts_flag, int tag_flag, char *fmt, ...);

/**
* @brief Print overall help message.
*/
void tpb_print_help_total(void);

/**
* @brief write 2d data with header into a csv file.
* @param path 
* @param data 
* @param nrow 
* @param ncol 
* @param header 
* @return int 
*/
int tpb_writecsv(char *path, int64_t **data, int nrow, int ncol, char *header);


int report_performance(uint64_t **ns, uint64_t **cy, uint64_t total_wall_time, int nskip, int ntest, int nepoch, int N, int Nr, int skip_comp, int skip_comm);

/***
 * Log every step's performance data into a csv file
 * the csv file will be named as "np${rank_size}_kernelname_ntest_N{N}.csv"
 * the csv headers are "rank0" ~ "rank${rank_size-1}"
 * each row is the performance data of a step
 * @param ns: the time data of each step
 * @param cy: the cycle data of each step
 * @param kernel_name: the name of the kernel
 * @param ntest: the number of steps
 * @param nepoch: the number of epochs
 * @param N: the matrix size
 * @param Nr: the number of rows to allreduce
 * @param skip_comp: whether to skip the computation
 * @return void
 */
int log_step_info(uint64_t **ns, uint64_t **cy, char *kernel_name, int ntest, int nepoch, int N, int Nr, int skip_comp, int skip_comm);