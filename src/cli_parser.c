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
 * @version 0.3
 * @brief tpbench command line parser 
 * @author Key Liao (keyliaohpc@gmail.com, keyliao@sjtu.edu.cn)
 * @date 2024-01-22
 */

#define _GNU_SOURCE

#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include "cli_parser.h"
#include "tperror.h"
#include "tpb_core.h"

/**
 * @brief 
 * @param key 
 * @param arg 
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
int parse_klist(__tp_args_t *tp_args);

/**
 * @brief 
 * @param tp_args 
 * @return int 
 */
int parse_glist(__tp_args_t *tp_args);

/**
 * @brief 
 * @param tp_args 
 * @return int 
 */
int init_list(__tp_args_t *tp_args);



int
check_count(int *n, char *strarg) {
    int len;
    char *ch;
    
    if(strarg[0] == '\0') {
        *n = 0;
        return NO_ERROR;
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
    return NO_ERROR;
}

int
parse_klist(__tp_args_t *tp_args) {
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
    return NO_ERROR;
}

int
parse_glist(__tp_args_t *tp_args) {
    int cmplen, matched;
    char *ch, *che;

    ch = tp_args->gstr;
    for(int seg = 0; seg < tp_args->ngrp; seg ++) {
        matched = 0;
        che = ch;
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
        for(int rid = 0; rid < ngrout; rid ++) {
            if(strcmp(ch, grp_info[rid].rname) == 0) {
                // matched, append index
                tp_args->glist[seg] = rid;
                matched = 1;
                break;
            }
        }
        if(matched == 0) {
            return GRP_NE;
        }
        ch = che + 1;
    }
    return NO_ERROR;
}

// extract tpbench arguments from string
int
init_list(__tp_args_t *tp_args) {
    int err;
    // syntax check
    if(err = check_count(&(tp_args->nkern), tp_args->kstr)) {
        return err;
    }
    if(err = check_count(&(tp_args->ngrp), tp_args->gstr)) {
        return err;
    }
    tp_args->klist = (int *)malloc(sizeof(int) * tp_args->nkern);
    tp_args->glist = (int *)malloc(sizeof(int) * tp_args->ngrp);
    if(tp_args->klist == NULL || tp_args->glist == NULL) {
        return MALLOC_FAIL;
    }
    // parse kstr
    err = parse_klist(tp_args);
    if(tp_args->nkern && err) {
        return err;
    }
    // parse gstr
    err = parse_glist(tp_args);
    if(tp_args->ngrp && err) {
        return err;
    }
    return NO_ERROR;
}

// ============================================================================
// define argp info

static error_t
parse_opt(int key, char *arg, struct argp_state *state) {
    __tp_args_t *args = state->input;
    switch(key) {
        case 'n':
            args->ntest = atoi(arg);
            break;
        case 's':
            args->nkib = atoi(arg);
            break;
        case 'g':
            if(strlen(arg) > 1023) {
                return SYNTAX_ERROR;
            }
            sprintf(args->gstr, "%s", arg);
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
parse_args(int argc, char **argv, __tp_args_t *tp_args) {
    int err;
    struct argp argp = {options, parse_opt, args_doc, doc};

    err = 0;
    if(argc <= 1) {
        printf("argc = %d\n", argc);
        return SYNTAX_ERROR;
    }
    // set default value
    tp_args->ntest = 0;
    tp_args->nkib = 0;
    tp_args->nkern = 0;
    tp_args->ngrp = 0;
    tp_args->list_only_flag = 0;
    tp_args->klist = NULL;
    tp_args->glist = NULL;
    tp_args->data_dir[0] = '\0';

    // parse by argp
    argp_parse(&argp, argc, argv, 0, 0, tp_args);

    // list only
    if(tp_args->list_only_flag) {
        return NO_ERROR;
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