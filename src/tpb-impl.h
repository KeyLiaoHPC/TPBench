/**
 * @file tpb-impl.h
 * @brief Implicit layer of multi-callback implementation.
 */
#ifndef TPB_IMPL_H
#define TPB_IMPL_H

#include "tpb-types.h"

// === Timers ===
#include "timers/timers.h"

// === Kernels ===
#include "kernels/kernels.h"

// === Errors ===
int tpb_get_err_exit_flag(int err);
const char *tpb_get_err_msg(int err);

#define __tpbm_exit_on_error(err, msg) \
    do { \
        if (err) { \
            tpb_printf(err, 1, 1, "%s. TPB message: %s", msg, tpb_get_err_msg(err)); \
            if (tpb_get_err_exit_flag(err) == 1) { \
                exit(err); \
            } \
        } \
    } while(0); \

#define DHLINE "================================================================================\n"
#define HLINE  "--------------------------------------------------------------------------------\n"

#endif