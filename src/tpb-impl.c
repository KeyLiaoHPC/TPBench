/*
 * @file tpb-impl.c
 * @brief Implementation of implicit layer of multi-callback implementation.
 */
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include "tpb-impl.h"

static tpb_error_type tpb_errors[] = {
    {TPBE_SUCCESS,          TPBE_NOTE,  "Action successful."},
    {TPBE_KERN_VERIFY_FAIL, TPBE_WARN,  "Kernel verification failed."},
    {TPBE_PE_NOT_BIND,      TPBE_WARN,  "Compute PE not binded."},
    {TPBE_EXIT_ON_HELP,     TPBE_FAIL,  "TPBench exits on help."},
    {GRP_ARG_ERROR,         TPBE_FAIL,  "GRP_ARG_ERROR"},
    {KERN_ARG_ERROR,        TPBE_FAIL,  "KERN_ARG_ERROR"},
    {TPBE_KERN_NE,          TPBE_FAIL,  "Kernel is not existed."},
    {GRP_NE,                TPBE_FAIL,  "GRP_NE"},
    {TPBE_CLI_SYNTAX_FAIL,  TPBE_FAIL,  "Command line syntax error."},
    {FILE_OPEN_FAIL,        TPBE_FAIL,  "FILE_OPEN_FAIL"},
    {MALLOC_FAIL,           TPBE_FAIL,  "MALLOC_FAIL"},
    {TPBE_ARGS_FAIL,        TPBE_FAIL,  "TPBE_ARGS_FAIL"},
    {MKDIR_ERROR,           TPBE_FAIL,  "MKDIR_ERROR"},
    {RES_INIT_FAIL,         TPBE_FAIL,  "RES_INIT_FAIL"},
    {TPBE_MPI_INIT_FAIL,    TPBE_FAIL,  "MPI initialization failed."},
};

int tpb_get_err_exit_flag(int err) {
    return tpb_errors[err].err_type;
}

const char *tpb_get_err_msg(int err) {
    return tpb_errors[err].err_msg;
}

int
tpb_char_is_legal_int(int64_t lower, int64_t upper, char *str)
{
    char *endptr;
    long long val;

    if(str == NULL) {
        return 0;
    }
    while(isspace((unsigned char)*str)) {
        str++;
    }
    if(*str == '\0') {
        return 0;
    }
    errno = 0;
    val = strtoll(str, &endptr, 10);
    if(errno == ERANGE) {
        return 0;
    }
    if(str == endptr) {
        return 0;
    }
    while(*endptr != '\0') {
        if(!isspace((unsigned char)*endptr)) {
            return 0;
        }
        endptr++;
    }
    if(val < lower || val > upper) {
        return 0;
    }
    return 1;
}

int
tpb_char_is_legal_fp(double lower, double upper, char *str)
{
    char *endptr;
    double val;

    if(str == NULL) {
        return 0;
    }
    while(isspace((unsigned char)*str)) {
        str++;
    }
    if(*str == '\0') {
        return 0;
    }
    errno = 0;
    val = strtod(str, &endptr);
    if(errno == ERANGE) {
        return 0;
    }
    if(str == endptr) {
        return 0;
    }
    while(*endptr != '\0') {
        if(!isspace((unsigned char)*endptr)) {
            return 0;
        }
        endptr++;
    }
    if(val < lower || val > upper) {
        return 0;
    }
    return 1;
}