/*
 * @file tpb-impl.c
 * @brief Implementation of implicit layer of multi-callback implementation.
 */
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include "tpb-impl.h"
#include "tpb-types.h"

static tpb_error_type tpb_errors[] = {
    {TPBE_SUCCESS,          TPBE_NOTE,  "Action successful."},
    {TPBE_EXIT_ON_HELP,     TPBE_FAIL,  "TPBench exits on help."},
    {TPBE_CLI_FAIL,         TPBE_FAIL,  "Failed to parse TPbench CLI argument."},
    {TPBE_FILE_IO_FAIL,     TPBE_FAIL,  "File I/O failed."},
    {TPBE_MALLOC_FAIL,      TPBE_FAIL,  "Memory allocation failed."},
    {TPBE_MPI_FAIL,         TPBE_FAIL,  "MPI initialization failed."},
    {TPBE_KERN_ARG_FAIL,    TPBE_FAIL,  "Failed to parse kernel arguments."},
    {TPBE_KERN_VERIFY_FAIL, TPBE_WARN,  "Kernel verification failed."},
    {TPBE_LIST_NOT_FOUND,   TPBE_FAIL,  "Element is not found in the list."},
    {TPBE_LIST_DUP,         TPBE_FAIL,  "Duplicated element."},
    {TPBE_NULLPTR_ARG,      TPBE_FAIL,  "Empty input pointer."},
    {TPBE_DTYPE_NOT_SUPPORTED, TPBE_FAIL, "Datatype not supported."},
    {TPBE_ILLEGAL_CALL, TPBE_FAIL, "Illegal call."}
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