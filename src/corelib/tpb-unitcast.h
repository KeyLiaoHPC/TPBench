/**
 * @file tpb-unitcast.h
 * @brief Unit casting functions for converting benchmark results to human-readable units.
 */

#ifndef TPB_UNITCAST_H
#define TPB_UNITCAST_H

#include <stddef.h>
#include "tpb-types.h"

/**
 * @brief Get scale factor to convert from base unit to given unit.
 *
 * For EXP-based units: scale = base^exponent
 * For MUL-based units: scale = multiplier value
 *
 * @param unit The target unit.
 * @return Scale factor (divide base unit value by this to get target unit value).
 */
double tpb_unit_get_scale(TPB_UNIT_T unit);

/**
 * @brief Cast array values to appropriate unit for human readability.
 *
 * @param arr Input array (any TPB_DTYPE).
 * @param narr Number of elements.
 * @param dtype Data type of input array.
 * @param unit_current Current unit of the values.
 * @param unit_cast Output: the unit after casting.
 * @param arr_cast Output: cast values as double array (must be pre-allocated).
 * @param sigbit Target significant figures.
 * @param decbit Maximum decimal places.
 * @return TPBE_SUCCESS or error code.
 */
int tpb_cast_unit(void *arr, int narr, TPB_DTYPE dtype,
                  TPB_UNIT_T unit_current, TPB_UNIT_T *unit_cast,
                  double *arr_cast, int sigbit, int decbit);

/**
 * @brief Format value with scientific notation.
 *
 * @param value The value to format.
 * @param buf Output buffer.
 * @param bufsize Buffer size.
 * @param sigbit Total significant figures.
 * @param intbit Integer digits before decimal point.
 * @return Number of characters written.
 */
int tpb_format_scientific(double value, char *buf, size_t bufsize,
                          int sigbit, int intbit);

/**
 * @brief Format value according to sigbit/intbit rules.
 *
 * @param value The value to format.
 * @param buf Output buffer.
 * @param bufsize Buffer size.
 * @param sigbit Total significant figures (0 or negative = no limit).
 * @param intbit Integer digits before decimal (0 or negative = no limit).
 * @return Number of characters written.
 */
int tpb_format_value(double value, char *buf, size_t bufsize,
                     int sigbit, int intbit);

/**
 * @brief Convert TPB_UNIT_T to human-readable string.
 *
 * @param unit The unit code.
 * @return Pointer to static string representation.
 */
const char *tpb_unit_to_string(TPB_UNIT_T unit);

#endif /* TPB_UNITCAST_H */
