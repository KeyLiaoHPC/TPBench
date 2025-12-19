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
#include "tpb-driver.h"
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
        return -1;
    }
    while(*ch != '\0') {
        if(!isalnum(*ch) && *ch != ',' && *ch != '_' && *ch != ':' && *ch != '=') {
            return -1;
        }
        if(*ch == ',' && *(ch+1) == ',') {
            return -1;
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
    tpb_kargs_token_t kargs_token;
    int err;
    int matched;

    // Use tpb_argstr_token to parse kernel list with arguments
    err = tpb_argstr_token(tpb_args->kstr, &kargs_token);
    if(err) {
        return err;
    }

    // Check if we have kernel format (kernel:args) or simple kernel list
    if(kargs_token.nkern == 0) {
        // Simple list format without colons, treat as kernel names only
        // This shouldn't happen with current logic, but handle it
        tpb_argstr_token_free(&kargs_token);
        return TPBE_CLI_ARG_FAIL;
    }

    // Match each kernel name to registered kernels
    for(int seg = 0; seg < kargs_token.nkern; seg++) {
        matched = 0;
        for(int rid = 0; rid < nkrout; rid++) {
            // Check both kname and rname for compatibility
            if(strcmp(kargs_token.kname[seg], kern_info[rid].kname) == 0 ||
               strcmp(kargs_token.kname[seg], kern_info[rid].rname) == 0) {
                tpb_args->klist[seg] = rid;
                matched = 1;
                break;
            }
        }
        if(matched == 0) {
            tpb_printf(TPBM_PRTN_M_DIRECT, "Kernel %s not found.\n", kargs_token.kname[seg]);
            tpb_argstr_token_free(&kargs_token);
            return 1;
        }
    }

    // Store kernel-specific arguments for later validation and application
    memcpy(&tpb_args->kargs_kernel, &kargs_token, sizeof(tpb_kargs_token_t));
    
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
tpb_argstr_token(const char *argstr, tpb_kargs_token_t *karg_token)
{
    char buf[TPBM_CLI_STR_MAX_LEN];
    char *saveptr_kern, *saveptr_arg;
    char *kernel_seg, *arg_token;
    int kernel_count = 0;
    int token_count = 0;
    int total_tokens = 0;
    int is_kernel_format = 0;
    char *first_colon, *first_comma, *first_eq;

    if(argstr == NULL || karg_token == NULL) {
        return TPBE_CLI_ARG_FAIL;
    }

    if(argstr[0] == '\0') {
        karg_token->nkern = 0;
        karg_token->ntoken = NULL;
        karg_token->kname = NULL;
        karg_token->token = NULL;
        return 0;
    }

    snprintf(buf, sizeof(buf), "%s", argstr);

    // Detect format: kernel-specific or common kargs
    // Kernel format: contains pattern like "name:arg=val" or "name,name2" or just "name"
    // Common format: only "arg=val,arg=val"
    first_colon = strchr(buf, ':');
    first_comma = strchr(buf, ',');
    first_eq = strchr(buf, '=');

    // If there's a colon before '=', it's kernel format
    // Or if there's no '=' at all, it's kernel format (simple kernel list)
    if((first_colon != NULL && (first_eq == NULL || first_colon < first_eq)) ||
       (first_eq == NULL)) {
        is_kernel_format = 1;
    }

    if(is_kernel_format) {
        // Kernel-specific format: kernel1:arg1=val1:arg2=val2,kernel2:arg3=val3
        // First pass: count kernels and total tokens
        char buf_copy[TPBM_CLI_STR_MAX_LEN];
        snprintf(buf_copy, sizeof(buf_copy), "%s", argstr);
        
        kernel_seg = strtok_r(buf_copy, ",", &saveptr_kern);
        while(kernel_seg != NULL) {
            kernel_count++;
            char *colon_pos = strchr(kernel_seg, ':');
            if(colon_pos != NULL) {
                // Count args for this kernel
                char *arg_start = colon_pos + 1;
                char *arg_copy = strdup(arg_start);
                char *arg_saveptr;
                char *arg = strtok_r(arg_copy, ":", &arg_saveptr);
                while(arg != NULL) {
                    if(strchr(arg, '=') != NULL) {
                        total_tokens++;
                    }
                    arg = strtok_r(NULL, ":", &arg_saveptr);
                }
                free(arg_copy);
            }
            kernel_seg = strtok_r(NULL, ",", &saveptr_kern);
        }

        karg_token->nkern = kernel_count;
        karg_token->ntoken = (int *)malloc(sizeof(int) * kernel_count);
        if(karg_token->ntoken == NULL) {
            return TPBE_MALLOC_FAIL;
        }

        karg_token->kname = (char **)malloc(sizeof(char *) * kernel_count);
        if(karg_token->kname == NULL) {
            free(karg_token->ntoken);
            return TPBE_MALLOC_FAIL;
        }

        karg_token->token = (char **)malloc(sizeof(char *) * total_tokens);
        if(karg_token->token == NULL) {
            free(karg_token->kname);
            free(karg_token->ntoken);
            return TPBE_MALLOC_FAIL;
        }

        // Second pass: extract kernel names and tokens
        snprintf(buf, sizeof(buf), "%s", argstr);
        kernel_count = 0;
        total_tokens = 0;

        kernel_seg = strtok_r(buf, ",", &saveptr_kern);
        while(kernel_seg != NULL) {
            token_count = 0;
            char *colon_pos = strchr(kernel_seg, ':');
            
            if(colon_pos != NULL) {
                // Extract kernel name
                *colon_pos = '\0';
                karg_token->kname[kernel_count] = strdup(kernel_seg);
                if(karg_token->kname[kernel_count] == NULL) {
                    // Cleanup
                    for(int i = 0; i < kernel_count; i++) {
                        free(karg_token->kname[i]);
                    }
                    for(int i = 0; i < total_tokens; i++) {
                        free(karg_token->token[i]);
                    }
                    free(karg_token->token);
                    free(karg_token->kname);
                    free(karg_token->ntoken);
                    return TPBE_MALLOC_FAIL;
                }
                
                // Extract args
                arg_token = strtok_r(colon_pos + 1, ":", &saveptr_arg);
                while(arg_token != NULL) {
                    if(strchr(arg_token, '=') != NULL) {
                        karg_token->token[total_tokens] = strdup(arg_token);
                        if(karg_token->token[total_tokens] == NULL) {
                            // Cleanup
                            for(int i = 0; i <= kernel_count; i++) {
                                free(karg_token->kname[i]);
                            }
                            for(int i = 0; i < total_tokens; i++) {
                                free(karg_token->token[i]);
                            }
                            free(karg_token->token);
                            free(karg_token->kname);
                            free(karg_token->ntoken);
                            return TPBE_MALLOC_FAIL;
                        }
                        total_tokens++;
                        token_count++;
                    }
                    arg_token = strtok_r(NULL, ":", &saveptr_arg);
                }
            } else {
                // No colon, just kernel name
                karg_token->kname[kernel_count] = strdup(kernel_seg);
                if(karg_token->kname[kernel_count] == NULL) {
                    // Cleanup
                    for(int i = 0; i < kernel_count; i++) {
                        free(karg_token->kname[i]);
                    }
                    for(int i = 0; i < total_tokens; i++) {
                        free(karg_token->token[i]);
                    }
                    free(karg_token->token);
                    free(karg_token->kname);
                    free(karg_token->ntoken);
                    return TPBE_MALLOC_FAIL;
                }
            }
            
            karg_token->ntoken[kernel_count] = token_count;
            kernel_count++;
            kernel_seg = strtok_r(NULL, ",", &saveptr_kern);
        }
    } else {
        // Common kargs format: arg1=val1,arg2=val2
        // First pass: count tokens
        char buf_copy[TPBM_CLI_STR_MAX_LEN];
        snprintf(buf_copy, sizeof(buf_copy), "%s", argstr);
        
        arg_token = strtok_r(buf_copy, ",", &saveptr_arg);
        while(arg_token != NULL) {
            if(strchr(arg_token, '=') != NULL) {
                total_tokens++;
            }
            arg_token = strtok_r(NULL, ",", &saveptr_arg);
        }

        karg_token->nkern = 0;
        karg_token->ntoken = (int *)malloc(sizeof(int));
        if(karg_token->ntoken == NULL) {
            return TPBE_MALLOC_FAIL;
        }
        karg_token->ntoken[0] = total_tokens;
        karg_token->kname = NULL;  // No kernel names for common kargs

        karg_token->token = (char **)malloc(sizeof(char *) * total_tokens);
        if(karg_token->token == NULL) {
            free(karg_token->ntoken);
            return TPBE_MALLOC_FAIL;
        }

        // Second pass: extract tokens
        snprintf(buf, sizeof(buf), "%s", argstr);
        token_count = 0;

        arg_token = strtok_r(buf, ",", &saveptr_arg);
        while(arg_token != NULL) {
            if(strchr(arg_token, '=') != NULL) {
                karg_token->token[token_count] = strdup(arg_token);
                if(karg_token->token[token_count] == NULL) {
                    // Cleanup
                    for(int i = 0; i < token_count; i++) {
                        free(karg_token->token[i]);
                    }
                    free(karg_token->token);
                    free(karg_token->ntoken);
                    return TPBE_MALLOC_FAIL;
                }
                token_count++;
            }
            arg_token = strtok_r(NULL, ",", &saveptr_arg);
        }
    }

    return 0;
}

void
tpb_argstr_token_free(tpb_kargs_token_t *karg_token)
{
    int total_tokens;

    if(karg_token == NULL) {
        return;
    }

    if(karg_token->ntoken != NULL) {
        if(karg_token->nkern == 0) {
            total_tokens = karg_token->ntoken[0];
        } else {
            total_tokens = 0;
            for(int i = 0; i < karg_token->nkern; i++) {
                total_tokens += karg_token->ntoken[i];
            }
        }

        if(karg_token->kname != NULL) {
            for(int i = 0; i < karg_token->nkern; i++) {
                if(karg_token->kname[i] != NULL) {
                    free(karg_token->kname[i]);
                }
            }
            free(karg_token->kname);
        }

        if(karg_token->token != NULL) {
            for(int i = 0; i < total_tokens; i++) {
                if(karg_token->token[i] != NULL) {
                    free(karg_token->token[i]);
                }
            }
            free(karg_token->token);
        }
        free(karg_token->ntoken);
    }

    karg_token->nkern = 0;
    karg_token->ntoken = NULL;
    karg_token->kname = NULL;
    karg_token->token = NULL;
}

int
tpb_parse_kargs_common(tpb_args_t *tpb_args, tpb_kargs_common_t *tpb_kargs)
{
    tpb_kargs_token_t karg_token;
    int err;
    int total_tokens;

    if(tpb_args == NULL || tpb_kargs == NULL) {
        return TPBE_CLI_ARG_FAIL;
    }

    if(tpb_args->kargstr[0] == '\0') {
        return 0;
    }

    // Tokenize the argument string
    err = tpb_argstr_token(tpb_args->kargstr, &karg_token);
    if(err) {
        return err;
    }
    // For common kargs, nkern should be 0
    if(karg_token.nkern != 0) {
        tpb_printf(TPBM_PRTN_M_DIRECT, 
                   "Invalid --kargs format. Expected key=value pairs.\n");
        tpb_argstr_token_free(&karg_token);
        return TPBE_CLI_ARG_FAIL;
    }
    total_tokens = karg_token.ntoken[0];
    for(int i = 0; i < total_tokens; i++) {
        char token_buf[TPBM_CLI_STR_MAX_LEN];
        char *eq;
        char *key;
        char *value;

        snprintf(token_buf, sizeof(token_buf), "%s", karg_token.token[i]);
        eq = strchr(token_buf, '=');

        if(eq == NULL) {
            tpb_printf(TPBM_PRTN_M_DIRECT, 
                       "Invalid --kargs entry \"%s\". Expected key=value.\n", 
                       karg_token.token[i]);
            tpb_argstr_token_free(&karg_token);
            return TPBE_CLI_ARG_FAIL;
        }

        *eq = '\0';
        key = tpb_trim_whitespace(token_buf);
        value = tpb_trim_whitespace(eq + 1);

        if(key == NULL || value == NULL || *key == '\0' || *value == '\0') {
            tpb_printf(TPBM_PRTN_M_DIRECT, 
                       "Invalid --kargs entry. Empty key or value detected.\n");
            tpb_argstr_token_free(&karg_token);
            return TPBE_CLI_ARG_FAIL;
        }

        if(strcmp(key, "ntest") == 0) {
            if(!tpb_char_is_legal_int(1, INT_MAX, value)) {
                tpb_printf(TPBM_PRTN_M_DIRECT, "Invalid ntest value: %s\n", value);
                tpb_argstr_token_free(&karg_token);
                return TPBE_CLI_ARG_FAIL;
            }
            tpb_kargs->ntest = (int)strtol(value, NULL, 10);
        } else if(strcmp(key, "nwarm") == 0) {
            if(!tpb_char_is_legal_int(0, INT_MAX, value)) {
                tpb_printf(TPBM_PRTN_M_DIRECT, "Invalid nwarm value: %s\n", value);
                tpb_argstr_token_free(&karg_token);
                return TPBE_CLI_ARG_FAIL;
            }
            tpb_kargs->nwarm = (int)strtol(value, NULL, 10);
        } else if(strcmp(key, "twarm") == 0) {
            if(!tpb_char_is_legal_int(0, INT_MAX, value)) {
                tpb_printf(TPBM_PRTN_M_DIRECT, "Invalid twarm value: %s\n", value);
                tpb_argstr_token_free(&karg_token);
                return TPBE_CLI_ARG_FAIL;
            }
            tpb_kargs->twarm = (int)strtol(value, NULL, 10);
        } else if(strcmp(key, "memsize") == 0) {
            if(!tpb_char_is_legal_int(1, INT64_MAX, value)) {
                tpb_printf(TPBM_PRTN_M_DIRECT, "Invalid memsize value: %s\n", value);
                tpb_argstr_token_free(&karg_token);
                return TPBE_CLI_ARG_FAIL;
            }
            tpb_kargs->memsize = (uint64_t)strtoll(value, NULL, 10);
        } else {
            // Warning for unsupported common args (they may be kernel-specific)
            tpb_printf(TPBM_PRTN_M_DIRECT, 
                       "Warning: Unknown common arg '%s' in --kargs (may be kernel-specific).\n", 
                       key);
        }
    }

    tpb_argstr_token_free(&karg_token);
    return 0;
}

int
tpb_validate_kernel_args(tpb_kargs_token_t *kargs_user, int kernel_id, 
                         tpb_kargs_common_t *kargs_common)
{
    tpb_kernel_t *kernel;
    tpb_kargs_token_t *kargs_def;
    int token_start = 0;
    
    if(kernel_id < 0 || kernel_id >= nkern) {
        return TPBE_KERN_NOT_FOUND;
    }
    
    kernel = &kernel_all[kernel_id];
    kargs_def = &kernel->kargs_def;
    
    // Calculate token start position for this kernel
    for(int i = 0; i < kernel_id; i++) {
        token_start += kargs_user->ntoken[i];
    }
    
    // Validate each user argument against supported parameters
    for(int i = 0; i < kargs_user->ntoken[kernel_id]; i++) {
        char token_buf[TPBM_CLI_STR_MAX_LEN];
        char *eq;
        char *key;
        char *value;
        int found = 0;
        
        snprintf(token_buf, sizeof(token_buf), "%s", kargs_user->token[token_start + i]);
        eq = strchr(token_buf, '=');
        
        if(eq == NULL) {
            tpb_printf(TPBM_PRTN_M_DIRECT, 
                       "Invalid kernel arg \"%s\". Expected key=value.\n", 
                       kargs_user->token[token_start + i]);
            return TPBE_KERN_ARG_FAIL;
        }
        
        *eq = '\0';
        key = tpb_trim_whitespace(token_buf);
        value = tpb_trim_whitespace(eq + 1);
        
        // Check if this key is supported by the kernel
        for(int j = 0; j < kargs_def->ntoken[0]; j++) {
            char def_buf[TPBM_CLI_STR_MAX_LEN];
            char *def_eq;
            char *def_key;
            
            snprintf(def_buf, sizeof(def_buf), "%s", kargs_def->token[j]);
            def_eq = strchr(def_buf, '=');
            if(def_eq != NULL) {
                *def_eq = '\0';
                def_key = tpb_trim_whitespace(def_buf);
                
                if(strcmp(key, def_key) == 0) {
                    found = 1;
                    
                    // Apply the value to kargs_common if it's a common parameter
                    if(strcmp(key, "ntest") == 0) {
                        if(!tpb_char_is_legal_int(1, INT_MAX, value)) {
                            tpb_printf(TPBM_PRTN_M_DIRECT, "Invalid ntest value: %s\n", value);
                            return TPBE_KERN_ARG_FAIL;
                        }
                        kargs_common->ntest = (int)strtol(value, NULL, 10);
                    } else if(strcmp(key, "nskip") == 0 || strcmp(key, "nwarm") == 0) {
                        if(!tpb_char_is_legal_int(0, INT_MAX, value)) {
                            tpb_printf(TPBM_PRTN_M_DIRECT, "Invalid %s value: %s\n", key, value);
                            return TPBE_KERN_ARG_FAIL;
                        }
                        kargs_common->nwarm = (int)strtol(value, NULL, 10);
                    } else if(strcmp(key, "twarm") == 0) {
                        if(!tpb_char_is_legal_int(0, INT_MAX, value)) {
                            tpb_printf(TPBM_PRTN_M_DIRECT, "Invalid twarm value: %s\n", value);
                            return TPBE_KERN_ARG_FAIL;
                        }
                        kargs_common->twarm = (int)strtol(value, NULL, 10);
                    } else if(strcmp(key, "memsize") == 0) {
                        if(!tpb_char_is_legal_int(1, INT64_MAX, value)) {
                            tpb_printf(TPBM_PRTN_M_DIRECT, "Invalid memsize value: %s\n", value);
                            return TPBE_KERN_ARG_FAIL;
                        }
                        kargs_common->memsize = (uint64_t)strtoll(value, NULL, 10);
                    } else if(strcmp(key, "dtype") == 0) {
                        // Validate dtype but don't store in kargs_common
                        if(strcmp(value, "double") != 0 && strcmp(value, "float") != 0) {
                            tpb_printf(TPBM_PRTN_M_DIRECT, 
                                       "Invalid dtype value: %s (supported: double, float)\n", value);
                            return TPBE_KERN_ARG_FAIL;
                        }
                    }
                    break;
                }
            }
        }
        
        if(!found) {
            tpb_printf(TPBM_PRTN_M_DIRECT, 
                       "Error: Unsupported kernel argument '%s' for kernel '%s'.\n", 
                       key, kernel->kname);
            return TPBE_KERN_ARG_FAIL;
        }
    }
    
    return 0;
}

int
tpb_get_kernel_info_list(tpb_args_t *tpb_args)
{
    int err;
    tpb_kargs_token_t kargs_token;

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
        // Parse kernel list with tpb_argstr_token to get kernel count
        err = tpb_argstr_token(tpb_args->kstr, &kargs_token);
        if(err) {
            return err;
        }
        
        tpb_args->nkern = kargs_token.nkern;
        tpb_argstr_token_free(&kargs_token);
        
        if(tpb_args->nkern == 0) {
            return 0;
        }
        
        tpb_args->klist = (int *)malloc(sizeof(int) * tpb_args->nkern);
        if(tpb_args->klist == NULL) {
            return TPBE_MALLOC_FAIL;
        }
        
        err = tpb_parse_klist(tpb_args);
        if(err) {
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
        return -1;
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
        return -1;
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
        tpb_printf(TPBM_PRTN_M_DIRECT, "No arguments provided, exit after printing help message.\n");
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
        memset(&tpb_args->kargs_kernel, 0, sizeof(tpb_kargs_token_t));
        tpb_set_timer("clock_gettime", tpb_args, timer);
        tpb_kargs->ntest = 10;
        tpb_kargs->nwarm = 2;
        tpb_kargs->twarm = 100;
        tpb_kargs->memsize = 32;
        for (int i = 2; i < argc; i ++) {
            if (strcmp(argv[i], "-k") == 0 || strcmp(argv[i], "--kernel_list") == 0) {
                if (i + 1 >= argc) {
                    tpb_printf(TPBM_PRTN_M_DIRECT, "Option %s requires arguments.\n", argv[i]);
                    return TPBE_CLI_ARG_FAIL;
                }
                if (strlen(argv[i + 1]) > TPBM_CLI_STR_MAX_LEN) {
                    tpb_printf(TPBM_PRTN_M_DIRECT, 
                                "Kernel list only supports up to %s characters.\n"
                                "Please use config file for complex tests.\n",
                                TPBM_CLI_STR_MAX_LEN);
                    return TPBE_CLI_ARG_FAIL;
                }
                sprintf(tpb_args->kstr, "%s", argv[++i]);
            } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--data_path") == 0) {
                if (i + 1 >= argc) {
                    tpb_printf(TPBM_PRTN_M_DIRECT, "Option %s requires an argument.\n", argv[i]);
                    return TPBE_CLI_ARG_FAIL;
                }
                if (strlen(argv[i + 1]) > PATH_MAX - 1) {
                    tpb_printf(TPBM_PRTN_M_DIRECT, "Data path string too long.\n");
                    return TPBE_CLI_ARG_FAIL;
                }
                sprintf(tpb_args->data_dir, "%s", argv[++i]);
            } else if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--timer") == 0) {
                if (i + 1 >= argc) {
                    tpb_printf(TPBM_PRTN_M_DIRECT, "Option %s requires arguments.\n", argv[i]);
                    return TPBE_CLI_ARG_FAIL;
                }
                err = tpb_set_timer(argv[++i], tpb_args, timer);
                if (err) {
                    tpb_printf(TPBM_PRTN_M_DIRECT, "Invalid timer: %s\n", argv[i]);
                    return TPBE_CLI_ARG_FAIL;
                }
            } else if (strcmp(argv[i], "--kargs") == 0) {
                if (i + 1 >= argc) {
                    tpb_printf(TPBM_PRTN_M_DIRECT, "Option %s requires arguments.\n", argv[i]);
                    return TPBE_CLI_ARG_FAIL;
                }
                if (strlen(argv[i + 1]) >= TPBM_CLI_STR_MAX_LEN) {
                    tpb_printf(TPBM_PRTN_M_DIRECT, "--kargs string too long.\n");
                    return TPBE_CLI_ARG_FAIL;
                }
                sprintf(tpb_args->kargstr, "%s", argv[++i]);
                err = tpb_parse_kargs_common(tpb_args, tpb_kargs);
                if (err) {
                    return TPBE_CLI_ARG_FAIL;
                }
            } else {
                tpb_printf(TPBM_PRTN_M_DIRECT, "Unknown option %s.\n", argv[i]);
                return TPBE_CLI_ARG_FAIL;
            }
        }
    } else if (strcmp(argv[1], "benchmark") == 0 ) {
    } else if (strcmp(argv[1], "list") == 0 ) {
    } else {
        tpb_printf(TPBM_PRTN_M_DIRECT, "Unsupported action: %s. Please use one of actions:\n"
                    "run, benchmark, list, help.\n", argv[1]);
        tpb_print_help_total();
        return TPBE_CLI_ARG_FAIL;
    }

    err = tpb_get_kernel_info_list(tpb_args);
    if(err) {
        return err;
    }
    
    // Validate and apply kernel-specific arguments (they override common args)
    if(tpb_args->kargs_kernel.nkern > 0) {
        for(int i = 0; i < tpb_args->nkern; i++) {
            err = tpb_validate_kernel_args(&tpb_args->kargs_kernel, i, tpb_kargs);
            if(err) {
                return err;
            }
        }
    }
    
    return 0;
}
