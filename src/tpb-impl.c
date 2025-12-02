/*
 * @file tpb-impl.c
 * @brief Implementation of implicit layer of multi-callback implementation.
 */
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
    {KERN_NE, 1,"KERN_NE"},
    {GRP_NE, 1,"GRP_NE"},
    {TPBE_CLI_SYNTAX_FAIL, 1,"Command line syntax error."},
    {FILE_OPEN_FAIL, 1,"FILE_OPEN_FAIL"},
    {MALLOC_FAIL, 1,"MALLOC_FAIL"},
    {ARGS_MISS, 1,"ARGS_MISS"},
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