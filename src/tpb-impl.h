/**
 * @file tpb-impl.h
 * @brief Implicit layer of multi-callback implementation.
 */
#ifndef TPB_IMPL_H
#define TPB_IMPL_H

#include "tpb-types.h"
#include "tpb-io.h"

// === Timers ===
#include "timers/timers.h"

// === Kernels ===
#include "kernels/kernels.h"

// === Errors ===
int tpb_get_err_exit_flag(int err);
const char *tpb_get_err_msg(int err);

/**
 * @brief Verify a string represents an integer within [lower, upper].
 * @return 1 if legal, 0 otherwise.
 */
int tpb_char_is_legal_int(int64_t lower, int64_t upper, char *str);

/**
 * @brief Verify a string represents a floating point within [lower, upper].
 * @return 1 if legal, 0 otherwise.
 */
int tpb_char_is_legal_fp(double lower, double upper, char *str);

#define __tpbm_exit_on_error(err, msg) \
    do { \
        if (err) { \
            unsigned __err_type = tpb_get_err_exit_flag(err); \
            tpb_printf(TPBM_PRTN_M_TSTAG | __err_type, "%s. Error message: %s\n", msg, tpb_get_err_msg(err)); \
            if (__err_type == TPBE_FAIL) { \
                exit(err); \
            } \
        } \
    } while(0)



#endif