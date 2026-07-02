/*
 * tpb-impl.c
 * Implementation of implicit layer of multi-callback implementation.
 */

#include <stdlib.h>
#include "tpb-impl.h"
#include "tpb-types.h"
#include "../include/tpb-public.h"

/* Error type definitions */
static tpb_error_type tpb_errors[] = {
    {TPBE_SUCCESS,             TPBE_NOTE, "Action successful."},
    {TPBE_EXIT_ON_HELP,        TPBE_FAIL, "TPBench exits on help."},
    {TPBE_CLI_FAIL,            TPBE_FAIL, "Failed to parse TPbench CLI argument."},
    {TPBE_FILE_IO_FAIL,        TPBE_FAIL, "File I/O failed."},
    {TPBE_MALLOC_FAIL,         TPBE_FAIL, "Memory allocation failed."},
    {TPBE_MPI_FAIL,            TPBE_FAIL, "MPI initialization failed."},
    {TPBE_KERN_ARG_FAIL,       TPBE_FAIL, "Failed to parse kernel arguments."},
    {TPBE_KERN_VERIFY_FAIL,    TPBE_WARN, "Kernel verification failed."},
    {TPBE_LIST_NOT_FOUND,      TPBE_FAIL, "Element is not found in the list."},
    {TPBE_LIST_DUP,            TPBE_FAIL, "Duplicated element."},
    {TPBE_NULLPTR_ARG,         TPBE_FAIL, "Empty input pointer."},
    {TPBE_DTYPE_NOT_SUPPORTED, TPBE_FAIL, "Datatype not supported."},
    {TPBE_ILLEGAL_CALL,        TPBE_FAIL, "Illegal call."},
    {TPBE_KERNEL_NE_FAIL,      TPBE_FAIL, "Kernel does not exist."},
    {TPBE_KARG_NE_FAIL,        TPBE_FAIL, "Kernel argument does not exist."},
    {TPBE_KERNEL_INCOMPLETE,   TPBE_FAIL, "Incomplete kernel (missing .so)."},
    {TPBE_DLOPEN_FAIL,         TPBE_FAIL, "Failed to load kernel library."},
    {TPBE_MERGE_MISMATCH,      TPBE_FAIL, "Task merge source mismatch."},
    {TPBE_MERGE_FAIL,          TPBE_FAIL, "Task merge failed."}
};

#define TPB_ERR_TABLE_SIZE ((int)(sizeof(tpb_errors) / sizeof(tpb_errors[0])))

static const tpb_error_type *
_sf_lookup_error(int err)
{
    int i;

    for (i = 0; i < TPB_ERR_TABLE_SIZE; i++) {
        if (tpb_errors[i].err_code == err) {
            return &tpb_errors[i];
        }
    }
    return &tpb_errors[0];
}

int
tpb_get_err_exit_flag(int err)
{
    return (int)_sf_lookup_error(err)->err_type;
}

const char *
tpb_get_err_msg(int err)
{
    return _sf_lookup_error(err)->err_msg;
}

int
tpb_report_error(int err, const char *context)
{
    unsigned err_type;

    if (err == TPBE_SUCCESS) {
        return TPBE_SUCCESS;
    }
    err_type = (unsigned)tpb_get_err_exit_flag(err);
    tpblog_printf_f(TPB_LOG_LEVEL_ERROR, err_type, TPBLOG_FLAG_TSTAG,
                    "%s. Error message: %s\n",
                    context != NULL ? context : "TPBench error",
                    tpb_get_err_msg(err));
    return err;
}

void
tpb_exit_on_error(int err, const char *context)
{
    if (err == TPBE_SUCCESS) {
        return;
    }
    (void)tpb_report_error(err, context);
    if (tpb_get_err_exit_flag(err) == TPBE_FAIL) {
        exit(err);
    }
}
