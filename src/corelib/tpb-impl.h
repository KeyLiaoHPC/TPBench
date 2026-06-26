/**
 * @file tpb-impl.h
 * @brief Implicit layer of multi-callback implementation.
 */

#ifndef TPB_IMPL_H
#define TPB_IMPL_H

#include "tpb-types.h"
#include "tpb-io.h"

/* Timers */
#include "../timers/timers.h"

/* Error handling functions */

/**
 * @brief Get error exit flag from error code.
 * @param err Error code.
 * @return Error type flag.
 */
int tpb_get_err_exit_flag(int err);

/**
 * @brief Get error message from error code.
 * @param err Error code.
 * @return Error message string.
 */
const char *tpb_get_err_msg(int err);

#define __tpbm_exit_on_error(err, msg) \
    do { \
        if (err) { \
            unsigned __err_type = tpb_get_err_exit_flag(err); \
            tpb_printf(TPBM_PRTN_M_TSTAG | __err_type, "%s. Error message: %s\n", \
                       msg, tpb_get_err_msg(err)); \
            if (__err_type == TPBE_FAIL) { \
                exit(err); \
            } \
        } \
    } while (0)

#endif /* TPB_IMPL_H */
