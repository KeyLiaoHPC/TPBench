/*
 * tpb-impl.c
 * Implementation of implicit layer of multi-callback implementation.
 */

#include <stdlib.h>
#include "tpb-impl.h"
#include "tpb-types.h"
#include "../include/tpb-public.h"

/* Local Function Prototypes */
static uint32_t _sf_log_tag_from_error(int err);

static tpb_error_type tpb_errors[] = {
    {TPBE_SUCCESS,             "Action successful."},
    {TPBE_EXIT_ON_HELP,        "TPBench exits on help."},
    {TPBE_CLI_FAIL,            "Failed to parse TPbench CLI argument."},
    {TPBE_FILE_IO_FAIL,        "File I/O failed."},
    {TPBE_MALLOC_FAIL,         "Memory allocation failed."},
    {TPBE_MPI_FAIL,            "MPI initialization failed."},
    {TPBE_KERN_ARG_FAIL,       "Failed to parse kernel arguments."},
    {TPBE_KERN_VERIFY_FAIL,    "Kernel verification failed."},
    {TPBE_LIST_NOT_FOUND,      "Element is not found in the list."},
    {TPBE_LIST_DUP,            "Duplicated element."},
    {TPBE_NULLPTR_ARG,         "Empty input pointer."},
    {TPBE_DTYPE_NOT_SUPPORTED, "Datatype not supported."},
    {TPBE_ILLEGAL_CALL,        "Illegal call."},
    {TPBE_KERNEL_NE_FAIL,      "Kernel does not exist."},
    {TPBE_KARG_NE_FAIL,        "Kernel argument does not exist."},
    {TPBE_KERNEL_INCOMPLETE,   "Incomplete kernel (missing .so)."},
    {TPBE_DLOPEN_FAIL,         "Failed to load kernel library."},
    {TPBE_METRIC_MISSING,      "Required benchmark metric not found in log."}
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

static uint32_t
_sf_log_tag_from_error(int err)
{
    if (err == TPBE_SUCCESS) {
        return TPB_LOG_TAG_NOTE;
    }
    if (err == TPBE_KERN_VERIFY_FAIL || err == TPBE_METRIC_MISSING) {
        return TPB_LOG_TAG_WARN;
    }
    if (_sf_lookup_error(err)->err_code == err) {
        return TPB_LOG_TAG_FAIL;
    }
    return TPB_LOG_TAG_NOTE;
}

const char *
tpb_get_err_msg(int err)
{
    return _sf_lookup_error(err)->err_msg;
}

/**
 * @brief Log a formatted error and return the error code.
 */
int
tpb_report_error(int err, const char *context)
{
    uint32_t log_tag;

    if (err == TPBE_SUCCESS) {
        return TPBE_SUCCESS;
    }
    log_tag = _sf_log_tag_from_error(err);
    tpblog_printf_f(TPB_LOG_LEVEL_ERROR, log_tag, TPBLOG_FLAG_TSTAG,
                    "%s. Error message: %s\n",
                    context != NULL ? context : "TPBench error",
                    tpb_get_err_msg(err));
    return err;
}
