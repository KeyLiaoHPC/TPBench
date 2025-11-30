/**
 * @file cli_parser.h
 * @brief Header for tpbench command line parser.
 */
#define _GNU_SOURCE

#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include "argp.h"
#include "tpb-types.h"

// argp option
static struct argp_option options[] = {
    {"ntest",       'n',    "# of test",    0, 
        "Overall number of tests."},
    {"nkib",        's',    "kib_size",     0, 
        "Memory usage for a single test array, in KiB."},
    {"kernel",      'k',    "kernel_list",  0,
        "Kernel list.(e.g. -k d_init,d_sum). "},
    {"list",        'L',    0,              0,
        "List all group and kernels then exit." },
    {"data_dir",    'd',    "PATH",         OPTION_ARG_OPTIONAL,
        "Optional. Data directory."},
    {"mode",        'm',    "MODE",         0,
        "Run mode: Supported modes: Manual (Default), BenchScore, BenchCompute, "
        "BenchMemory, BenchNetwork, BenchIO."},
    {"timer",       't',    "timer_name",    0, 
        "Timer name: Supported timers: clock_gettime (default), tsc_asym."},
    { 0 }
};

/**
 * @brief parse cli arguments 
 * @param argc int, argc of main()
 * @param argv char**, argv of main()
 * @param tp_args tpb_args_t*，pointer to argument data struct
 * @param timer tpb_timer_t*，pointer to timer data struct
 * @return int 
 */
int parse_args(int argc, char **argv, tpb_args_t *tp_args, tpb_timer_t *timer);

/**
 * @brief 
 * @param key 
 * @param arg 
 * @param timer 
 * @param state 
 * @return error_t 
 */
 static error_t parse_opt(int key, char *arg, struct argp_state *state);

 /**
  * @brief check syntax of aruments while counting segments.
  * @param strarg 
  * @return int 
  */
 int check_count(int *n, char *strarg);
 
 /**
  * @brief 
  * @param tp_args 
  * @return int 
  */
 int parse_klist(tpb_args_t *tp_args);
 
 /**
  * @brief 
  * @param tp_args 
  * @return int 
  */
 int init_list(tpb_args_t *tp_args);