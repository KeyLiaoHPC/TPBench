/**
 * =================================================================================
 * TPBench - A throughputs benchmarking tool for high-performance computing
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
 * =================================================================================
 * @file tpb-cli.c
 * @version 0.71
 * @brief tpbench command line parser 
 * @author Key Liao (keyliaohpc@gmail.com, keyliao@sjtu.edu.cn)
 * @date 2024-01-22
 */
#include <linux/limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <ctype.h>
#include "tpb-cli.h"
#include "timers/timers.h"
#include "tpb-impl.h"
#include "tpb-core.h"
#include "tpb-io.h"
#include "tpb-types.h"

int
tpb_check_count(int *n, char *strarg)
{
    int len;
    char *ch;
    
    if(strarg[0] == '\0') {
        *n = 0;
        return 0;
    }
    len = strlen(strarg);
    if(strarg[len-1] == ',') {
        strarg[len-1] = '\0';
    }
    *n = 1;
    ch = strarg;
    if(!isalnum(*ch)) {
        return TPBE_CLI_SYNTAX_FAIL;
    }
    while(*ch != '\0') {
        if(!isalnum(*ch) && *ch != ',' && *ch != '_') {
            return TPBE_CLI_SYNTAX_FAIL;
        }
        if(*ch == ',' && *(ch+1) == ',') {
            return TPBE_CLI_SYNTAX_FAIL;
        }
        if(*ch == ',') {
            (*n) += 1;
        }
        ch++;
    }
    return 0;
}

int
tpb_parse_klist(tpb_args_t *tpb_args)
{
    int matched;
    char *ch, *che;

    ch = tpb_args->kstr;
    for(int seg = 0; seg < tpb_args->nkern; seg++) {
        matched = 0;
        che = ch;
        while(1) {
            if(*che == ',') {
                *che = '\0';
                break;
            }
            if(*che == '\0') {
                break;
            }
            che++;
        }
        for(int rid = 0; rid < nkrout; rid++) {
            if(strcmp(ch, kern_info[rid].rname) == 0) {
                tpb_args->klist[seg] = rid;
                matched = 1;
                break;
            }
        }
        if(matched == 0) {
            return TPBE_KERN_NE;
        }
        ch = che + 1;
    }
    return 0;
}

static char *
tpb_trim_whitespace(char *str)
{
    char *end;

    if(str == NULL) {
        return NULL;
    }

    while(*str && isspace((unsigned char)*str)) {
        str++;
    }

    if(*str == '\0') {
        return str;
    }

    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) {
        *end = '\0';
        end--;
    }
    if(isspace((unsigned char)*end)) {
        *end = '\0';
    }

    return str;
}

int
tpb_parse_kargs_common(tpb_args_t *tpb_args, tpb_kargs_common_t *tpb_kargs)
{
    char buf[TPBM_CLI_STR_MAX_LEN];
    char *token, *saveptr;

    if(tpb_args == NULL || tpb_kargs == NULL) {
        return TPBE_ARGS_FAIL;
    }

    if(tpb_args->kargstr[0] == '\0') {
        return 0;
    }

    snprintf(buf, sizeof(buf), "%s", tpb_args->kargstr);
    saveptr = NULL;
    token = strtok_r(buf, ",", &saveptr);
    while(token != NULL) {
        char *eq = strchr(token, '=');
        char *key;
        char *value;

        if(eq == NULL) {
            tpb_printf(TPBM_PRTN_M_DIRECT, "Invalid --kargs entry \"%s\". Expected key=value.", token);
            return TPBE_ARGS_FAIL;
        }

        *eq = '\0';
        key = tpb_trim_whitespace(token);
        value = tpb_trim_whitespace(eq + 1);

        if(key == NULL || value == NULL || *key == '\0' || *value == '\0') {
            tpb_printf(TPBM_PRTN_M_DIRECT, "Invalid --kargs entry. Empty key or value detected.");
            return TPBE_ARGS_FAIL;
        }

        if(strcmp(key, "ntest") == 0) {
            if(!tpb_char_is_legal_int(1, INT_MAX, value)) {
                tpb_printf(TPBM_PRTN_M_DIRECT, "Invalid ntest value: %s", value);
                return TPBE_ARGS_FAIL;
            }
            tpb_kargs->ntest = (int)strtol(value, NULL, 10);
        } else if(strcmp(key, "nwarm") == 0) {
            if(!tpb_char_is_legal_int(0, INT_MAX, value)) {
                tpb_printf(TPBM_PRTN_M_DIRECT, "Invalid nwarm value: %s", value);
                return TPBE_ARGS_FAIL;
            }
            tpb_kargs->nwarm = (int)strtol(value, NULL, 10);
        } else if(strcmp(key, "twarm") == 0) {
            if(!tpb_char_is_legal_int(0, INT_MAX, value)) {
                tpb_printf(TPBM_PRTN_M_DIRECT, "Invalid twarm value: %s", value);
                return TPBE_ARGS_FAIL;
            }
            tpb_kargs->twarm = (int)strtol(value, NULL, 10);
        } else if(strcmp(key, "memsize") == 0) {
            if(!tpb_char_is_legal_int(1, INT64_MAX, value)) {
                tpb_printf(TPBM_PRTN_M_DIRECT, "Invalid memsize value: %s", value);
                return TPBE_ARGS_FAIL;
            }
            tpb_kargs->memsize = (uint64_t)strtoll(value, NULL, 10);
        } else {
            tpb_printf(TPBM_PRTN_M_DIRECT, "Unsupported --kargs key: %s", key);
            return TPBE_ARGS_FAIL;
        }

        token = strtok_r(NULL, ",", &saveptr);
    }

    return 0;
}

int
tpb_init_list(tpb_args_t *tpb_args)
{
    int err;

    if(tpb_args->mode == 1) {
        // BenchScore mode: including compute/memory/network.
        // Compute subsystem: fmaldr, mulldr.
        // Memory subsystem: init, copy, scale, striad.
        // Network subsystem: MPI_Send/Recv, MPI_Allreduce.
    } else if(tpb_args->mode == 2) {
        // BenchCompute mode
    } else if(tpb_args->mode == 3) {
        // BenchMemory mode
    } else if(tpb_args->mode == 4) {
        // BenchNetwork mode
    } else if(tpb_args->mode == 5) {
        // BenchIO mode
    } else {
        if((err = tpb_check_count(&(tpb_args->nkern), tpb_args->kstr))) {
            return err;
        }
        tpb_args->klist = (int *)malloc(sizeof(int) * tpb_args->nkern);
        if(tpb_args->klist == NULL) {
            return MALLOC_FAIL;
        }
        err = tpb_parse_klist(tpb_args);
        if(tpb_args->nkern && err) {
            return err;
        }
        return 0;
    }
    return 0;
}

static int
tpb_set_mode(tpb_args_t *args, const char *arg)
{
    if(strcmp(arg, "BenchScore") == 0) {
        args->mode = 1;
    } else if(strcmp(arg, "BenchCompute") == 0) {
        args->mode = 2;
    } else if(strcmp(arg, "BenchMemory") == 0) {
        args->mode = 3;
    } else if(strcmp(arg, "BenchNetwork") == 0) {
        args->mode = 4;
    } else if(strcmp(arg, "BenchIO") == 0) {
        args->mode = 5;
    } else {
        return TPBE_CLI_SYNTAX_FAIL;
    }
    return 0;
}

static int
tpb_set_timer(const char *arg, tpb_args_t *args, tpb_timer_t *timer)
{
    if(strcmp(arg, "clock_gettime") == 0) {
        sprintf(args->timer, "clock_gettime");
        timer->init = init_timer_clock_gettime;
        timer->tick = tick_clock_gettime;
        timer->tock = tock_clock_gettime;
        timer->get_stamp = get_time_clock_gettime;
    } else if(strcmp(arg, "tsc_asym") == 0) {
        sprintf(args->timer, "tsc_asym");
        timer->init = init_timer_tsc_asym;
        timer->tick = tick_tsc_asym;
        timer->tock = tock_tsc_asym;
        timer->get_stamp = get_time_tsc_asym;
    } else {
        return TPBE_CLI_SYNTAX_FAIL;
    }
    return 0;
}

int
tpb_parse_args( int argc, 
                char **argv, 
                tpb_args_t *tpb_args, 
                tpb_kargs_common_t *tpb_kargs,
                tpb_timer_t *timer)
{
    int opt;
    int err = 0;

    if(argc <= 1) {
        // No args.
        tpb_printf(TPBM_PRTN_M_DIRECT, "No arguments provided, exit after printing help message.");
        tpb_print_help_total();
        return TPBE_EXIT_ON_HELP;
    }

    if (strcmp(argv[1], "help") == 0) {
        if (argc <= 2) tpb_print_help_total();
        return TPBE_EXIT_ON_HELP;
    } else if (strcmp(argv[1], "run") == 0 ) {
        // Set default.
        tpb_args->mode = 0;
        tpb_args->nkern = 0;
        tpb_args->list_only_flag = 0;
        tpb_args->klist = NULL;
        strcpy(tpb_args->data_dir, "./data");
        tpb_args->kstr[0] = '\0';
        tpb_args->kargstr[0] = '\0';
        tpb_set_timer("clock_gettime", tpb_args, timer);
        tpb_kargs->ntest = 10;
        tpb_kargs->nwarm = 2;
        tpb_kargs->twarm = 100;
        tpb_kargs->memsize = 32;
        for (int i = 2; i < argc; i ++) {
            if (strcmp(argv[i], "-k") == 0 || strcmp(argv[i], "--kernel_list") == 0) {
                if (i + 1 >= argc) {
                    tpb_printf(TPBM_PRTN_M_DIRECT, "Option %s requires arguments.", argv[i]);
                    err = TPBE_ARGS_FAIL;
                    break;
                }
                if (strlen(argv[i + 1]) > TPBM_CLI_STR_MAX_LEN) {
                    tpb_printf(TPBM_PRTN_M_DIRECT, 
                                "Kernel list only supports up to %s characters.\n"
                                "Please use config file for complex tests.",
                                TPBM_CLI_STR_MAX_LEN);
                    err = TPBE_CLI_SYNTAX_FAIL;
                    break;
                }
                sprintf(tpb_args->kstr, "%s", argv[++i]);
            } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--data_path") == 0) {
                if (i + 1 >= argc) {
                    tpb_printf(TPBM_PRTN_M_DIRECT, "Option %s requires an argument.", argv[i]);
                    err = TPBE_ARGS_FAIL;
                    break;
                }
                if (strlen(argv[i + 1]) > PATH_MAX - 1) {
                    tpb_printf(TPBM_PRTN_M_DIRECT, "Data path string too long.");
                    err = TPBE_CLI_SYNTAX_FAIL;
                    break;
                }
                sprintf(tpb_args->data_dir, "%s", argv[++i]);
            } else if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--timer") == 0) {
                if (i + 1 >= argc) {
                    tpb_printf(TPBM_PRTN_M_DIRECT, "Option %s requires arguments.", argv[i]);
                    err = TPBE_ARGS_FAIL;
                    break;
                }
                err = tpb_set_timer(argv[++i], tpb_args, timer);
                if (err) {
                    tpb_printf(TPBM_PRTN_M_DIRECT, "Invalid timer: %s", argv[i]);
                    break;
                }
            } else if (strcmp(argv[i], "--kargs") == 0) {
                if (i + 1 >= argc) {
                    tpb_printf(TPBM_PRTN_M_DIRECT, "Option %s requires arguments.", argv[i]);
                    err = TPBE_ARGS_FAIL;
                    break;
                }
                if (strlen(argv[i + 1]) >= TPBM_CLI_STR_MAX_LEN) {
                    tpb_printf(TPBM_PRTN_M_DIRECT, "--kargs string too long.");
                    err = TPBE_CLI_SYNTAX_FAIL;
                    break;
                }
                sprintf(tpb_args->kargstr, "%s", argv[++i]);
                err = tpb_parse_kargs_common(tpb_args, tpb_kargs);
                if (err) {
                    break;
                }
            } else {
                tpb_printf(TPBM_PRTN_M_DIRECT, "Unknown option %s.", argv[i]);
                err = TPBE_CLI_SYNTAX_FAIL;
                break;
            }
        }
    } else if (strcmp(argv[1], "benchmark") == 0 ) {
    } else if (strcmp(argv[1], "list") == 0 ) {
    } else {
        tpb_printf(TPBM_PRTN_M_DIRECT, "Unsupported action: %s. Please use one of actions:\n"
                    "run, benchmark, list, help.", argv[1]);
        tpb_print_help_total();
        return TPBE_CLI_SYNTAX_FAIL;
    }

    err = tpb_init_list(tpb_args);
    return err;
}
