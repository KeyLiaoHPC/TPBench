/**
 * =================================================================================
 * TPBench - A throughputs benchmarking tool for high-performance computing
 *
 * Copyright (C) 2024 Key Liao (Liao Qiucheng)
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE. See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see https://www.gnu.org/licenses/.
 * =================================================================================
 * @file tpb-unitcast.h
 * @brief Unit casting functions for converting benchmark results to human-readable units.
 * @author Key Liao (keyliaohpc@gmail.com)
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
 * @param unit  The target unit
 * @return      Scale factor (divide base unit value by this to get target unit value)
 */
double tpb_unit_get_scale(TPB_UNIT_T unit);

/**
 * @brief Cast array values to appropriate unit for human readability.
 *
 * Casting routes by unit name:
 *   - TPB_UNAME_BITSIZE:  Cast via BIN_EXP (bit->byte->KiB->MiB...)
 *   - TPB_UNAME_DATASIZE: Cast via DEC_EXP (B->KB->MB->GB...)
 *   - TPB_UNAME_WALLTIME: Cast via DEC_EXP (ns->us->ms->s)
 *   - TPB_UNAME_PHYSTIME: Cast via DEC_EXP (cy->Kcy->Mcy...)
 *   - TPB_UNAME_DATETIME: Cast via DEC_MUL (sec->min->hour->day...)
 *   - TPB_UNAME_FLOPS:    Cast via DEC_EXP (FLOP/s->KFLOP/s...)
 *   - TPB_UNAME_OP:       Cast via DEC_EXP (OP->KOP->MOP...)
 *   - TPB_UNAME_OPS:      Cast via DEC_EXP (op/s->Kop/s...)
 *   - TPB_UNAME_TOKENPS:  Cast via DEC_EXP (token/s->Ktoken/s...)
 *   - TPB_UNAME_TPS:      Cast via DEC_EXP (tps->Ktps...)
 *   - TPB_UNAME_UNKNTIME: No casting
 *   - TPB_UNAME_GRIDSIZE: No casting
 *
 * @param arr          Input array (any TPB_DTYPE)
 * @param narr         Number of elements
 * @param dtype        Data type of input array
 * @param unit_current Current unit of the values
 * @param unit_cast    [out] The unit after casting
 * @param arr_cast     [out] Cast values as double array (must be pre-allocated)
 * @param sigbit       Target significant figures
 * @param decbit       Maximum decimal places
 * @return             TPBE_SUCCESS or error code
 */
int tpb_cast_unit(void *arr, int narr, TPB_DTYPE dtype,
                  TPB_UNIT_T unit_current, TPB_UNIT_T *unit_cast,
                  double *arr_cast, int sigbit, int decbit);

/**
 * @brief Format value with scientific notation.
 *
 * Used when unit casting reaches end-unit or is not applicable.
 * Format: <integer_part>.<decimal_part>E<exponent>
 *
 * @param value        The value to format
 * @param buf          Output buffer
 * @param bufsize      Buffer size
 * @param sigbit       Total significant figures
 * @param decbit       Decimal places in mantissa
 * @return             Number of characters written
 */
int tpb_format_scientific(double value, char *buf, size_t bufsize,
                          int sigbit, int decbit);

/**
 * @brief Convert TPB_UNIT_T to human-readable string.
 *
 * @param unit The unit code.
 * @return Pointer to static string representation.
 */
const char *tpb_unit_to_string(TPB_UNIT_T unit);

#endif /* TPB_UNITCAST_H */

