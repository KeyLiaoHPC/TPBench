/*
 * @file tpb-impl.c
 * @brief Implementation of implicit layer of multi-callback implementation.
 */
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include "tpb-impl.h"

static tpb_error_type tpb_errors[] = {
    {TPBE_SUCCESS, 0, "Action successful."},
    {TPBE_WARNING, 0,"TPB_WARNING"},
    {VERIFY_FAIL, 0,"TPB_VERIFY_FAIL"},
    {OVER_OPTMIZE, 0,"TPB_OVER_OPTMIZE"},
    {DEFAULT_DIR, 0,"TPB_DEFAULT_DIR"},
    {CORE_NOT_BIND, 0,"TPB_CORE_NOT_BIND"},
    {TPBE_FAIL, 1,"TPB_FAIL"},
    {TPBE_EXIT_ON_HELP, 1,"TPBE_EXIT_ON_HELP"},
    {GRP_ARG_ERROR, 1,"GRP_ARG_ERROR"},
    {KERN_ARG_ERROR, 1,"KERN_ARG_ERROR"},
    {TPBE_KERN_NE, 1,"Kernel is not existed."},
    {GRP_NE, 1,"GRP_NE"},
    {TPBE_CLI_SYNTAX_FAIL, 1,"Command line syntax error."},
    {FILE_OPEN_FAIL, 1,"FILE_OPEN_FAIL"},
    {MALLOC_FAIL, 1,"MALLOC_FAIL"},
    {TPBE_ARGS_FAIL, 1,"TPBE_ARGS_FAIL"},
    {MKDIR_ERROR, 1,"MKDIR_ERROR"},
    {RES_INIT_FAIL, 1,"RES_INIT_FAIL"},
    {TPBE_MPI_INIT_FAIL, 1,"MPI initialization failed."},
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