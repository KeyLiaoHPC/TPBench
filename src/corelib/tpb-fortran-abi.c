/*
 * tpb-fortran-abi.c
 * Typed wrappers so Fortran ISO_C_BINDING can call the public kernel API.
 */

#include "tpb-public.h"

/**
 * @brief Call timer->tick if the timer is valid.
 */
int
tpb_timer_tick(tpb_timer_t *timer, int64_t *ts)
{
    if (timer == NULL || ts == NULL || timer->tick == NULL) {
        TPB_FAIL(TPB_MOD_DRIVER, TPBE_NULLPTR_ARG, NULL);
    }
    timer->tick(ts);
    return TPBE_SUCCESS;
}

/**
 * @brief Call timer->tock if the timer is valid.
 */
int
tpb_timer_tock(tpb_timer_t *timer, int64_t *ts)
{
    if (timer == NULL || ts == NULL || timer->tock == NULL) {
        TPB_FAIL(TPB_MOD_DRIVER, TPBE_NULLPTR_ARG, NULL);
    }
    timer->tock(ts);
    return TPBE_SUCCESS;
}

/**
 * @brief Convert a tock elapsed value to seconds.
 */
int
tpb_timer_elapsed_seconds(const tpb_timer_t *timer, int64_t elapsed,
                          double *seconds)
{
    TPB_UNIT_T raw;
    TPB_UNIT_T uname;

    if (timer == NULL || seconds == NULL) {
        TPB_FAIL(TPB_MOD_DRIVER, TPBE_NULLPTR_ARG, NULL);
    }

    uname = timer->unit & TPB_UNAME_MASK;
    if (uname != TPB_UNAME_WALLTIME) {
        TPB_FAIL(TPB_MOD_DRIVER, TPBE_KERN_ARG_FAIL, NULL);
    }

    raw = timer->unit & (TPB_UNAME_MASK | TPB_UBASE_MASK | TPB_UNIT_MASK);
    if (raw == (TPB_UNIT_NS &
                (TPB_UNAME_MASK | TPB_UBASE_MASK | TPB_UNIT_MASK))) {
        *seconds = (double)elapsed * 1.0e-9;
        return TPBE_SUCCESS;
    }
    if (raw == (TPB_UNIT_US &
                (TPB_UNAME_MASK | TPB_UBASE_MASK | TPB_UNIT_MASK))) {
        *seconds = (double)elapsed * 1.0e-6;
        return TPBE_SUCCESS;
    }
    if (raw == (TPB_UNIT_MS &
                (TPB_UNAME_MASK | TPB_UBASE_MASK | TPB_UNIT_MASK))) {
        *seconds = (double)elapsed * 1.0e-3;
        return TPBE_SUCCESS;
    }
    if (raw == (TPB_UNIT_SS &
                (TPB_UNAME_MASK | TPB_UBASE_MASK | TPB_UNIT_MASK))) {
        *seconds = (double)elapsed;
        return TPBE_SUCCESS;
    }

    TPB_FAIL(TPB_MOD_DRIVER, TPBE_KERN_ARG_FAIL, NULL);
}

/**
 * @brief Register a CLI int64 argument with range check (no varargs).
 */
int
tpb_k_add_arg_i64_range(const char *name, const char *tag,
                        const char *note, const char *default_val,
                        int64_t lo, int64_t hi)
{
    return tpb_k_add_arg(name, tag, note, default_val,
        TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_RANGE, lo, hi);
}

/**
 * @brief Register a CLI int64 argument with a value list (no varargs).
 */
int
tpb_k_add_arg_i64_list(const char *name, const char *tag,
                       const char *note, const char *default_val,
                       int n, const int64_t *list)
{
    if (n < 0 || (n > 0 && list == NULL)) {
        TPB_FAIL(TPB_MOD_DRIVER, TPBE_NULLPTR_ARG, NULL);
    }
    return tpb_k_add_arg(name, tag, note, default_val,
        TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_LIST, n, list);
}

/**
 * @brief Register a CLI char argument with a value list (no varargs).
 */
int
tpb_k_add_arg_char_list(const char *name, const char *tag,
                        const char *note, const char *default_val,
                        int n, const char *list)
{
    if (n < 0 || (n > 0 && list == NULL)) {
        TPB_FAIL(TPB_MOD_DRIVER, TPBE_NULLPTR_ARG, NULL);
    }
    return tpb_k_add_arg(name, tag, note, default_val,
        TPB_PARM_CLI | TPB_CHAR_T | TPB_PARM_LIST, n, list);
}

/**
 * @brief Read a registered int64 argument.
 */
int
tpb_k_get_arg_i64(const char *name, int64_t *out)
{
    if (out == NULL) {
        TPB_FAIL(TPB_MOD_DRIVER, TPBE_NULLPTR_ARG, NULL);
    }
    return tpb_k_get_arg(name, TPB_INT64_T, out);
}

/**
 * @brief Read a registered char argument.
 */
int
tpb_k_get_arg_char(const char *name, char *out)
{
    if (out == NULL) {
        TPB_FAIL(TPB_MOD_DRIVER, TPBE_NULLPTR_ARG, NULL);
    }
    return tpb_k_get_arg(name, TPB_CHAR_T, out);
}

/**
 * @brief Read a registered double argument.
 */
int
tpb_k_get_arg_double(const char *name, double *out)
{
    if (out == NULL) {
        TPB_FAIL(TPB_MOD_DRIVER, TPBE_NULLPTR_ARG, NULL);
    }
    return tpb_k_get_arg(name, TPB_DOUBLE_T, out);
}
