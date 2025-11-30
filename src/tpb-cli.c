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
 * @file tpb_parser.c
 * @version 0.71
 * @brief tpbench command line parser 
 * @author Key Liao (keyliaohpc@gmail.com, keyliao@sjtu.edu.cn)
 * @date 2024-01-22
 */
#include <stdlib.h>
#include <limits.h>
#include "tpb-cli.h"
#include "timers/timers.h"
#include "tpb-impl.h"
#include "tpb-core.h"

int
check_count(int *n, char *strarg) {
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
    // don't start with comma(',')
    if(!isalnum(*ch)) {
        // not start with alphanumeric
        return SYNTAX_ERROR;
    }
    // don't use character other than alphanumeric or comma.
    // don't use two concessive cooma.
    // ending with comma is allowed.
    while(*ch != '\0') {
        if(!isalnum(*ch) && *ch != ',' && *ch != '_') {
            // not alphanumeric, underscore or comma
            return SYNTAX_ERROR;
        }
        if(*ch == ',' && *(ch+1) == ',') {
            // two consecutive comma
            return SYNTAX_ERROR;
        }
        if(*ch == ',') {
            (*n) += 1;
        }
        ch ++;
    }
    return 0;
}

int
parse_klist(tpb_args_t *tp_args) {
    int cmplen, matched;
    char *ch, *che;

    ch = tp_args->kstr;
    for(int seg = 0; seg < tp_args->nkern; seg ++) {
        matched = 0;
        che = ch;
        // set segment
        while (1) {
            if(*che == ',') {
                *che = '\0';
                break;
            }
            if(*che == '\0') {
                break;
            }
            che ++;
            // printf("%s\n", che);
        }
        // route id
        for(int rid = 0; rid < nkrout; rid ++) {
            if(strcmp(ch, kern_info[rid].rname) == 0) {
                tp_args->klist[seg] = rid;
                matched = 1;
                break;
            }
        }
        if(matched == 0) {
            return KERN_NE;
        }
        // move to next segment
        ch = che + 1;
    }
    return 0;
}

// extract tpbench arguments from string
int
init_list(tpb_args_t *tp_args) {
    int err;
    // If mode is set, use mode-specific kernel and/or group list.
    // Otherwise, read kernel and group list from command line.
    if(tp_args->mode == 1) {
        // BenchScore mode: including compute/memory/network.
        // Compute subsystem: fmaldr, mulldr.
        // Memory subsystem: init, copy, scale, striad.
        // Network subsystem: MPI_Send/Recv, MPI_Allreduce.
        
    } else if(tp_args->mode == 2) {
        // BenchCompute mode
    } else if(tp_args->mode == 3) {
        // BenchMemory mode
    } else if(tp_args->mode == 4) {
        // BenchNetwork mode
    } else if(tp_args->mode == 5) {
        // BenchIO mode
    } else {
        // syntax check
        if ((err = check_count(&(tp_args->nkern), tp_args->kstr))) {
            return err;
        }
        tp_args->klist = (int *)malloc(sizeof(int) * tp_args->nkern);
        if(tp_args->klist == NULL) {
            return MALLOC_FAIL;
        }
        // parse kstr
        err = parse_klist(tp_args);
        if(tp_args->nkern && err) {
            return err;
        }

        return 0;
    }
    return 0;
}

// ============================================================================
// define argp info

static error_t
parse_opt(int key, char *arg, struct argp_state *state) {
    tpb_args_t *args = state->input;

    sprintf(args->timer, "%s", "clock_gettime");
    switch(key) {
        case 'n':
            args->ntest = atoi(arg);
            break;
        case 's':
            args->nkib = atoi(arg);
            break;
        case 'k':
            if(strlen(arg) > 1023) {
                return SYNTAX_ERROR;
            }
            sprintf(args->kstr, "%s", arg);
            break;
        case 'L':
            args->list_only_flag = 1;
            break;
        case 'o':
            if(strlen(arg) > 1023) {
                return SYNTAX_ERROR;
            }
            sprintf(args->data_dir, "%s", arg);
            break;
        case 'm':
            if (strcmp(arg, "BenchScore") == 0) {
                args->mode = 1;
            } else if (strcmp(arg, "BenchCompute") == 0) {
                args->mode = 2;
            } else if (strcmp(arg, "BenchMemory") == 0) {
                args->mode = 3;
            } else if (strcmp(arg, "BenchNetwork") == 0) {
                args->mode = 4;
            } else if (strcmp(arg, "BenchIO") == 0) {
                args->mode = 5;
            } else {
                return SYNTAX_ERROR;
            }
            break;
        case 't':
            if(strcmp(arg, "clock_gettime") == 0) {
                sprintf(args->timer, "clock_gettime");
            } else if(strcmp(arg, "tsc_asym") == 0) {
                sprintf(args->timer, "tsc_asym");
            } else {
                return SYNTAX_ERROR;
            }
            break;
        case ARGP_KEY_ARG:
            argp_usage(state);
            break;
        case ARGP_KEY_END:
            break;
        default:
          return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

const char *argp_program_version = "TPBench-v0.3, maintaining by Key Liao, "
    "Center for HPC, Shanghai Jiao Tong University";
const char *argp_program_bug_address = "keyliao@sjtu.edu.cn";
static char doc[] = "TPBench - A parallel high-precision throughputs "
    "benchmarking tool for high-performance computing.";
static char args_doc[] = "";

// end argp define
// ============================================================================

// entry of parser.
int
parse_args(int argc, char **argv, tpb_args_t *tp_args, tpb_timer_t *timer) {
    int err = 0;
    struct argp argp = {options, parse_opt, args_doc, doc};

    if(argc <= 1) {
        printf("argc = %d, argv = %s\n", argc, argv[0]);
        return SYNTAX_ERROR;
    }
    // set default value
    tp_args->mode = 0;
    tp_args->ntest = 0;
    tp_args->nkib = 0;
    tp_args->nkern = 0;
    tp_args->list_only_flag = 0;
    tp_args->klist = NULL;
    tp_args->data_dir[0] = '\0';

    // parse by argp
    err = argp_parse(&argp, argc, argv, 0, 0, tp_args);
    if(err) {
        return err;
    }

    // Set timer
    if(strcmp(tp_args->timer, "clock_gettime") == 0) {
        timer->init = init_timer_clock_gettime;
        timer->tick = tick_clock_gettime;
        timer->tock = tock_clock_gettime;
        timer->get_stamp = get_time_clock_gettime;
    } else if(strcmp(tp_args->timer, "tsc_asym") == 0) {
        timer->init = init_timer_tsc_asym;
        timer->tick = tick_tsc_asym;
        timer->tock = tock_tsc_asym;
        timer->get_stamp = get_time_tsc_asym;
    }

    // list only
    if(tp_args->list_only_flag) {
        return 0;
    }

    // argument integrity check
    if(tp_args->ntest == 0 || tp_args->nkib == 0) {
        return ARGS_MISS;
    }

    // Default data path, ./data
    if(strlen(tp_args->data_dir) == 0) {
        sprintf(tp_args->data_dir, "./data");
        err = DEFAULT_DIR;
    }
    // gen kid and gid list
    err = init_list(tp_args);

    return err;
}