/*
 * tpb-impl.c
 * Implementation of implicit layer of multi-callback implementation.
 */

#include <stdlib.h>
#include <string.h>
#include "tpb-impl.h"
#include "tpb-types.h"
#include "tpblog/tpb-printf.h"
#include "../include/tpb-public.h"

/* Local Function Prototypes */
static const char *_sf_basename(const char *path);
static uint32_t _sf_log_tag_from_cause(int cause);

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

static const char *tpb_module_names[] = {
    "NONE",
    "DRIVER",
    "ARGP",
    "IMPL",
    "IO",
    "MISC",
    "RAF_L1",
    "RAF_L2_KERNEL",
    "RAF_L2_TASK",
    "RAF_L2_TBATCH",
    "RAF_L2_RTENV",
    "RAF_L3_KERNEL",
    "RAF_L3_TASK",
    "RAF_L3_TBATCH",
    "RAF_L3_RTENV",
    "RAF_MISC",
    "CLI_RUN",
    "CLI_BENCHMARK",
    "CLI_KERNEL",
    "CLI_MISC",
    "CLI_RTENV"
};

#define TPB_ERR_TABLE_SIZE ((int)(sizeof(tpb_errors) / sizeof(tpb_errors[0])))
#define TPB_MOD_NAME_COUNT ((int)(sizeof(tpb_module_names) / sizeof(tpb_module_names[0])))

static const tpb_error_type *
_sf_lookup_cause(int cause)
{
    int i;

    for (i = 0; i < TPB_ERR_TABLE_SIZE; i++) {
        if (tpb_errors[i].err_code == cause) {
            return &tpb_errors[i];
        }
    }
    return NULL;
}

static const char *
_sf_basename(const char *path)
{
    const char *slash;

    if (path == NULL) {
        return "?";
    }
    slash = strrchr(path, '/');
    if (slash != NULL && slash[1] != '\0') {
        return slash + 1;
    }
    return path;
}

static uint32_t
_sf_log_tag_from_cause(int cause)
{
    if (cause == TPBE_SUCCESS) {
        return TPB_LOG_TAG_NOTE;
    }
    if (cause == TPBE_KERN_VERIFY_FAIL || cause == TPBE_METRIC_MISSING) {
        return TPB_LOG_TAG_WARN;
    }
    if (_sf_lookup_cause(cause) != NULL) {
        return TPB_LOG_TAG_FAIL;
    }
    return TPB_LOG_TAG_FAIL;
}

const char *
tpb_get_err_msg(int err)
{
    const tpb_error_type *entry;

    entry = _sf_lookup_cause(TPBE_CAUSE(err));
    if (entry != NULL) {
        return entry->err_msg;
    }
    return "Unknown error.";
}

const char *
tpb_err_module_name(int mod)
{
    if (mod >= 0 && mod < TPB_MOD_NAME_COUNT) {
        return tpb_module_names[mod];
    }
    return "UNKNOWN";
}

int
tpb_err_propagate(int cur_mod, int err)
{
    int cause;

    if (err == TPBE_SUCCESS || err == TPBE_EXIT_ON_HELP) {
        return err;
    }
    cause = TPBE_CAUSE(err);
    return TPBE_MAKE(cur_mod, cause);
}

int
tpb_err_to_exit_status(int err)
{
    if (err == TPBE_EXIT_ON_HELP) {
        return 0;
    }
    return TPBE_CAUSE(err);
}

/**
 * @brief Log an error at file/func and return TPBE_MAKE(mod, cause).
 */
int
tpb_report_error_at(const char *file, const char *func, int mod,
                    int cause, int is_origin, const char *msg)
{
    const char *reason;
    const char *base;
    uint32_t log_tag;

    if (cause == TPBE_SUCCESS) {
        return TPBE_SUCCESS;
    }

    base = _sf_basename(file);
    reason = tpb_get_err_msg(cause);
    log_tag = _sf_log_tag_from_cause(cause);

    if (is_origin) {
        if (msg != NULL && msg[0] != '\0') {
            tpblog_printf_f(TPB_LOG_LEVEL_ERROR, log_tag, TPBLOG_FLAG_TSTAG,
                            "At %s, %s. %s. %s [errcode=%d].\n",
                            base, func, msg, reason, cause);
        } else {
            tpblog_printf_f(TPB_LOG_LEVEL_ERROR, log_tag, TPBLOG_FLAG_TSTAG,
                            "At %s, %s. %s [errcode=%d].\n",
                            base, func, reason, cause);
        }
    } else if (msg != NULL && msg[0] != '\0') {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, log_tag, TPBLOG_FLAG_TSTAG,
                        "At %s, %s. %s [errcode=%d].\n",
                        base, func, msg, cause);
    } else {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, log_tag, TPBLOG_FLAG_TSTAG,
                        "At %s, %s [errcode=%d].\n",
                        base, func, cause);
    }

    return TPBE_MAKE(mod, cause);
}
