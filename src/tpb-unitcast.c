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
 * @file tpb-unitcast.c
 * @brief Unit casting implementation for converting benchmark results to
 *        human-readable units.
 * @author Key Liao (keyliaohpc@gmail.com)
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include "tpb-unitcast.h"
#include "tpb-types.h"
#include "tpb-stat.h"

/* ============================================================================
 * Unit Progression Tables
 * ============================================================================ */

/* BITSIZE (binary): BIT -> BYTE -> KiB -> MiB -> GiB -> TiB -> PIB -> EiB */
static const TPB_UNIT_T bitsize_bin[] = {
    TPB_UNIT_BIT, TPB_UNIT_BYTE, TPB_UNIT_KIB, TPB_UNIT_MIB,
    TPB_UNIT_GIB, TPB_UNIT_TIB, TPB_UNIT_PIB, TPB_UNIT_EIB
};
static const int bitsize_bin_len = 8;

/* DATASIZE (decimal): B -> KB -> MB -> GB -> TB -> PB -> EB -> ZB -> YB */
static const TPB_UNIT_T datasize_dec[] = {
    TPB_UNIT_B, TPB_UNIT_KB, TPB_UNIT_MB, TPB_UNIT_GB,
    TPB_UNIT_TB, TPB_UNIT_PB, TPB_UNIT_EB, TPB_UNIT_ZB, TPB_UNIT_YB
};
static const int datasize_dec_len = 9;

/* WALLTIME (decimal): ns -> us -> ms -> s */
static const TPB_UNIT_T walltime_dec[] = {
    TPB_UNIT_NS, TPB_UNIT_US, TPB_UNIT_MS, TPB_UNIT_SS
};
static const int walltime_dec_len = 4;

/* PHYSTIME (decimal): cy -> Kcy -> Mcy -> Gcy -> Tcy -> Pcy -> Ecy -> Zcy -> Ycy */
static const TPB_UNIT_T phystime_dec[] = {
    TPB_UNIT_CY, TPB_UNIT_KCY, TPB_UNIT_MCY, TPB_UNIT_GCY,
    TPB_UNIT_TCY, TPB_UNIT_PCY, TPB_UNIT_ECY, TPB_UNIT_ZCY, TPB_UNIT_YCY
};
static const int phystime_dec_len = 9;

/* DATETIME (multiplier): sec -> min -> hour -> day -> month -> year */
static const TPB_UNIT_T datetime_mul[] = {
    TPB_UNIT_SEC, TPB_UNIT_MIN, TPB_UNIT_HOU,
    TPB_UNIT_DAY, TPB_UNIT_MON, TPB_UNIT_YEA
};
static const int datetime_mul_len = 6;

/* OP (decimal): op -> Kop -> Mop -> Gop -> Top -> Pop -> Eop -> Zop -> Yop */
static const TPB_UNIT_T op_dec[] = {
    TPB_UNIT_OP, TPB_UNIT_KOP, TPB_UNIT_MOP, TPB_UNIT_GOP,
    TPB_UNIT_TOP, TPB_UNIT_POP, TPB_UNIT_EOP, TPB_UNIT_ZOP, TPB_UNIT_YOP
};
static const int op_dec_len = 9;

/* OPS (decimal): op/s -> Kop/s -> Mop/s -> Gop/s -> ... */
static const TPB_UNIT_T ops_dec[] = {
    TPB_UNIT_OPS, TPB_UNIT_KOPS, TPB_UNIT_MOPS, TPB_UNIT_GOPS,
    TPB_UNIT_TOPS, TPB_UNIT_POPS, TPB_UNIT_EOPS, TPB_UNIT_ZOPS, TPB_UNIT_YOPS
};
static const int ops_dec_len = 9;

/* FLOPS (decimal): FLOP/s -> KFLOP/s -> MFLOP/s -> ... */
static const TPB_UNIT_T flops_dec[] = {
    TPB_UNIT_FLOPS, TPB_UNIT_KFLOPS, TPB_UNIT_MFLOPS, TPB_UNIT_GFLOPS,
    TPB_UNIT_TFLOPS, TPB_UNIT_PFLOPS, TPB_UNIT_EFLOPS, TPB_UNIT_ZFLOPS, TPB_UNIT_YFLOPS
};
static const int flops_dec_len = 9;

/* TOKENPS (decimal): token/s -> Ktoken/s -> ... */
static const TPB_UNIT_T tokenps_dec[] = {
    TPB_UNIT_TOKENPS, TPB_UNIT_KTOKENPS, TPB_UNIT_MTOKENPS, TPB_UNIT_GTOKENPS,
    TPB_UNIT_TTOKENPS, TPB_UNIT_PTOKENPS, TPB_UNIT_ETOKENPS, TPB_UNIT_ZTOKENPS, TPB_UNIT_YTOKENPS
};
static const int tokenps_dec_len = 9;

/* TPS (decimal): tps -> Ktps -> Mtps -> ... */
static const TPB_UNIT_T tps_dec[] = {
    TPB_UNIT_TPS, TPB_UNIT_KTPS, TPB_UNIT_MTPS, TPB_UNIT_GTPS,
    TPB_UNIT_TTPS, TPB_UNIT_PTPS, TPB_UNIT_ETPS, TPB_UNIT_ZTPS, TPB_UNIT_YTPS
};
static const int tps_dec_len = 9;

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get element from array as double, based on dtype.
 */
static double
get_as_double(void *arr, int idx, TPB_DTYPE dtype)
{
    TPB_DTYPE type_only = dtype & TPB_PARM_TYPE_MASK;

    switch (type_only) {
    case TPB_INT_T:
    case TPB_INT32_T:
        return (double)((int32_t *)arr)[idx];
    case TPB_INT8_T:
        return (double)((int8_t *)arr)[idx];
    case TPB_INT16_T:
        return (double)((int16_t *)arr)[idx];
    case TPB_INT64_T:
        return (double)((int64_t *)arr)[idx];
    case TPB_UINT8_T:
        return (double)((uint8_t *)arr)[idx];
    case TPB_UINT16_T:
        return (double)((uint16_t *)arr)[idx];
    case TPB_UINT32_T:
        return (double)((uint32_t *)arr)[idx];
    case TPB_UINT64_T:
        return (double)((uint64_t *)arr)[idx];
    case TPB_FLOAT_T:
        return (double)((float *)arr)[idx];
    case TPB_DOUBLE_T:
        return ((double *)arr)[idx];
    default:
        return 0.0;
    }
}

/**
 * @brief Extract unit name from unit code.
 */
static inline TPB_UNIT_T
extract_uname(TPB_UNIT_T unit)
{
    return (unit & TPB_UNAME_MASK) >> 36;
}

/**
 * @brief Extract unit kind from unit code.
 */
static inline TPB_UNIT_T
extract_ukind(TPB_UNIT_T unit)
{
    return (unit & TPB_UKIND_MASK) >> 44;
}

/**
 * @brief Extract base type from unit code.
 */
static inline uint32_t
extract_ubase(TPB_UNIT_T unit)
{
    return (uint32_t)((unit >> 32) & 0xF);
}

/* ============================================================================
 * Scale Factor Calculation
 * ============================================================================ */

double
tpb_unit_get_scale(TPB_UNIT_T unit)
{
    uint32_t value = (uint32_t)(unit & TPB_UNIT_MASK);
    uint32_t base_type = extract_ubase(unit);

    switch (base_type) {
    case 0x1:  /* BASE - no conversion */
        return 1.0;
    case 0x2:  /* BIN_EXP_P: 2^value */
        return pow(2.0, (double)value);
    case 0x3:  /* BIN_EXP_N: 2^(-value) */
        return pow(2.0, -(double)value);
    case 0x4:  /* BIN_MUL_P: direct binary multiplier */
        return (value == 0) ? 1.0 : (double)value;
    case 0x5:  /* OCT_EXP_P: 8^value */
        return pow(8.0, (double)value);
    case 0x6:  /* OCT_EXP_N: 8^(-value) */
        return pow(8.0, -(double)value);
    case 0x7:  /* DEC_EXP_P: 10^value */
        return pow(10.0, (double)value);
    case 0x8:  /* DEC_EXP_N: 10^(-value) */
        return pow(10.0, -(double)value);
    case 0x9:  /* DEC_MUL_P: direct multiplier */
        return (value == 0) ? 1.0 : (double)value;
    case 0xa:  /* HEX_EXP_P: 16^value */
        return pow(16.0, (double)value);
    case 0xb:  /* HEX_EXP_N: 16^(-value) */
        return pow(16.0, -(double)value);
    default:
        return 1.0;
    }
}

/* ============================================================================
 * Unit Casting
 * ============================================================================ */

/**
 * @brief Find target unit index in table based on minimum value and sigbit.
 *
 * For EXP-based units, find the largest unit where the scaled value
 * has <= sigbit significant figures.
 */
static int
find_target_unit_exp(const TPB_UNIT_T *table, int len, double min_in_base, int sigbit)
{
    if (min_in_base <= 0.0 || len == 0) {
        return 0;
    }

    int target_idx = 0;

    for (int i = len - 1; i >= 0; i--) {
        double scale = tpb_unit_get_scale(table[i]);
        double scaled_val = min_in_base / scale;

        if (scaled_val >= 1.0) {
            /* Check significant figures: count integer digits */
            int int_digits = (int)floor(log10(fabs(scaled_val))) + 1;
            if (int_digits <= sigbit) {
                target_idx = i;
                break;
            }
        }
    }

    return target_idx;
}

/**
 * @brief Find target unit index for MUL-based units (DATETIME).
 *
 * Find the largest unit where scaled value >= 1.
 */
static int
find_target_unit_mul(const TPB_UNIT_T *table, int len, double min_in_base, int sigbit)
{
    if (min_in_base <= 0.0 || len == 0) {
        return 0;
    }

    int target_idx = 0;

    for (int i = len - 1; i >= 0; i--) {
        double scale = tpb_unit_get_scale(table[i]);
        double scaled_val = min_in_base / scale;

        if (scaled_val >= 1.0) {
            /* Check significant figures */
            int int_digits = (int)floor(log10(fabs(scaled_val))) + 1;
            if (int_digits <= sigbit) {
                target_idx = i;
                break;
            }
        }
    }

    return target_idx;
}

/**
 * @brief Check if unit belongs to a specific table by searching for it.
 */
static int
unit_in_table(TPB_UNIT_T unit, const TPB_UNIT_T *table, int len)
{
    for (int i = 0; i < len; i++) {
        if (unit == table[i]) {
            return 1;
        }
    }
    return 0;
}

/**
 * @brief Select unit table based on unit value.
 *
 * Check unit membership by searching each table directly.
 */
static int
select_table(TPB_UNIT_T unit, const TPB_UNIT_T **table, int *len)
{
    /* Check each table for membership */
    if (unit_in_table(unit, bitsize_bin, bitsize_bin_len)) {
        *table = bitsize_bin;
        *len = bitsize_bin_len;
        return 1;
    }
    if (unit_in_table(unit, datasize_dec, datasize_dec_len)) {
        *table = datasize_dec;
        *len = datasize_dec_len;
        return 1;
    }
    if (unit_in_table(unit, walltime_dec, walltime_dec_len)) {
        *table = walltime_dec;
        *len = walltime_dec_len;
        return 1;
    }
    if (unit_in_table(unit, phystime_dec, phystime_dec_len)) {
        *table = phystime_dec;
        *len = phystime_dec_len;
        return 1;
    }
    if (unit_in_table(unit, datetime_mul, datetime_mul_len)) {
        *table = datetime_mul;
        *len = datetime_mul_len;
        return 1;
    }
    if (unit_in_table(unit, op_dec, op_dec_len)) {
        *table = op_dec;
        *len = op_dec_len;
        return 1;
    }
    if (unit_in_table(unit, ops_dec, ops_dec_len)) {
        *table = ops_dec;
        *len = ops_dec_len;
        return 1;
    }
    if (unit_in_table(unit, flops_dec, flops_dec_len)) {
        *table = flops_dec;
        *len = flops_dec_len;
        return 1;
    }
    if (unit_in_table(unit, tokenps_dec, tokenps_dec_len)) {
        *table = tokenps_dec;
        *len = tokenps_dec_len;
        return 1;
    }
    if (unit_in_table(unit, tps_dec, tps_dec_len)) {
        *table = tps_dec;
        *len = tps_dec_len;
        return 1;
    }

    /* Not castable: UNKNTIME, GRIDSIZE */
    *table = NULL;
    *len = 0;
    return 0;
}

int
tpb_cast_unit(void *arr, int narr, TPB_DTYPE dtype,
              TPB_UNIT_T unit_current, TPB_UNIT_T *unit_cast,
              double *arr_cast, int sigbit, int decbit)
{
    if (arr == NULL || unit_cast == NULL || arr_cast == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    if (narr <= 0) {
        *unit_cast = unit_current;
        return TPBE_SUCCESS;
    }

    /*
     * Special handling for TPB_UNIT_TIMER which shouldn't be cast.
     */
    if (unit_current == TPB_UNIT_TIMER) {
        for (int i = 0; i < narr; i++) {
            arr_cast[i] = get_as_double(arr, i, dtype);
        }
        *unit_cast = unit_current;
        return TPBE_SUCCESS;
    }

    /* Select table based on unit name */
    const TPB_UNIT_T *table;
    int table_len;
    int castable = select_table(unit_current, &table, &table_len);

    if (!castable || table == NULL || table_len == 0) {
        /* Not castable - copy values as-is */
        for (int i = 0; i < narr; i++) {
            arr_cast[i] = get_as_double(arr, i, dtype);
        }
        *unit_cast = unit_current;
        return TPBE_SUCCESS;
    }

    /* Find minimum value */
    double min_val;
    tpb_stat_min(arr, narr, dtype, &min_val);

    /* Convert min to base unit */
    double current_scale = tpb_unit_get_scale(unit_current);
    double min_in_base = fabs(min_val) * current_scale;

    /* Find target unit */
    uint32_t base_type = extract_ubase(unit_current);
    int target_idx;

    if (base_type == 0x9) {
        /* DEC_MUL_P - use multiplier-based search */
        target_idx = find_target_unit_mul(table, table_len, min_in_base, sigbit);
    } else {
        /* EXP-based - use exponent-based search */
        target_idx = find_target_unit_exp(table, table_len, min_in_base, sigbit);
    }

    /* Scale all values */
    TPB_UNIT_T target_unit = table[target_idx];
    double target_scale = tpb_unit_get_scale(target_unit);

    for (int i = 0; i < narr; i++) {
        double val = get_as_double(arr, i, dtype);
        double val_in_base = val * current_scale;
        arr_cast[i] = val_in_base / target_scale;
    }

    *unit_cast = target_unit;
    return TPBE_SUCCESS;
}

int
tpb_format_scientific(double value, char *buf, size_t bufsize,
                      int sigbit, int intbit)
{
    if (buf == NULL || bufsize == 0) {
        return 0;
    }

    /* Calculate precision: sigbit total figures, with intbit integer part */
    int precision = sigbit - intbit;
    if (precision < 0) {
        precision = 0;
    }

    return snprintf(buf, bufsize, "%.*E", precision, value);
}

int
tpb_format_value(double value, char *buf, size_t bufsize,
                 int sigbit, int intbit)
{
    if (buf == NULL || bufsize == 0) {
        return 0;
    }

    /* Case 4: sigbit<=0 && intbit<=0 - No formatting, print as-is */
    if (sigbit <= 0 && intbit <= 0) {
        return snprintf(buf, bufsize, "%g", value);
    }

    /* Handle zero specially */
    if (value == 0.0) {
        if (sigbit > 0) {
            return snprintf(buf, bufsize, "0.%0*d", sigbit - 1, 0);
        }
        return snprintf(buf, bufsize, "0");
    }

    double absval = fabs(value);
    int value_int_digits = (absval >= 1.0) ? (int)floor(log10(absval)) + 1 : 0;

    /* Case 2: sigbit>0 && intbit<=0 - Format as 0.XXXXXeE */
    if (sigbit > 0 && intbit <= 0) {
        /* All sigbit digits after decimal point */
        return snprintf(buf, bufsize, "%.*e", sigbit - 1, value);
    }

    /* Case 3: sigbit<=0 && intbit>0 - Limit integer digits only */
    if (sigbit <= 0 && intbit > 0) {
        if (value_int_digits > intbit) {
            /* Exceeds intbit, use scientific notation */
            int exp_adjust = value_int_digits - intbit;
            double mantissa = value / pow(10.0, exp_adjust);
            return snprintf(buf, bufsize, "%.6gE%d", mantissa, exp_adjust);
        }
        return snprintf(buf, bufsize, "%g", value);
    }

    /* Case 1: sigbit>0 && intbit>0 - Format as XXX.YYY */
    int dec_digits = sigbit - intbit;
    if (dec_digits < 0) {
        dec_digits = 0;
    }

    if (value_int_digits > intbit) {
        /* Exceeds intbit integer digits, use XXX.YYYE format */
        int exp_adjust = value_int_digits - intbit;
        double mantissa = value / pow(10.0, exp_adjust);
        return snprintf(buf, bufsize, "%.*fE%d", dec_digits, mantissa, exp_adjust);
    }

    /* Normal formatting with sigbit significant figures */
    return snprintf(buf, bufsize, "%.*f", dec_digits, value);
}

/* ============================================================================
 * Unit to String Conversion
 * ============================================================================ */

const char *
tpb_unit_to_string(TPB_UNIT_T unit)
{
    /* BITSIZE units */
    if (unit == TPB_UNIT_BIT) return "bit";
    if (unit == TPB_UNIT_BYTE) return "B";
    if (unit == TPB_UNIT_KIB) return "KiB";
    if (unit == TPB_UNIT_MIB) return "MiB";
    if (unit == TPB_UNIT_GIB) return "GiB";
    if (unit == TPB_UNIT_TIB) return "TiB";
    if (unit == TPB_UNIT_PIB) return "PiB";
    if (unit == TPB_UNIT_EIB) return "EiB";
    if (unit == TPB_UNIT_ZIB) return "ZiB";
    if (unit == TPB_UNIT_YIB) return "YiB";

    /* DATASIZE units */
    if (unit == TPB_UNIT_B) return "B";
    if (unit == TPB_UNIT_KB) return "KB";
    if (unit == TPB_UNIT_MB) return "MB";
    if (unit == TPB_UNIT_GB) return "GB";
    if (unit == TPB_UNIT_TB) return "TB";
    if (unit == TPB_UNIT_PB) return "PB";
    if (unit == TPB_UNIT_EB) return "EB";
    if (unit == TPB_UNIT_ZB) return "ZB";
    if (unit == TPB_UNIT_YB) return "YB";

    /* WALLTIME units */
    if (unit == TPB_UNIT_NS) return "ns";
    if (unit == TPB_UNIT_US) return "us";
    if (unit == TPB_UNIT_MS) return "ms";
    if (unit == TPB_UNIT_SS) return "s";
    if (unit == TPB_UNIT_PS) return "ps";
    if (unit == TPB_UNIT_FS) return "fs";
    if (unit == TPB_UNIT_AS) return "as";

    /* PHYSTIME units */
    if (unit == TPB_UNIT_CY) return "cy";
    if (unit == TPB_UNIT_KCY) return "Kcy";
    if (unit == TPB_UNIT_MCY) return "Mcy";
    if (unit == TPB_UNIT_GCY) return "Gcy";
    if (unit == TPB_UNIT_TCY) return "Tcy";
    if (unit == TPB_UNIT_PCY) return "Pcy";
    if (unit == TPB_UNIT_ECY) return "Ecy";
    if (unit == TPB_UNIT_ZCY) return "Zcy";
    if (unit == TPB_UNIT_YCY) return "Ycy";

    /* PHYSTIME units */
    if (unit == TPB_UNIT_TICK) return "tick";
    if (unit == TPB_UNIT_KTICK) return "Ktick";
    if (unit == TPB_UNIT_MTICK) return "Mtick";
    if (unit == TPB_UNIT_GTICK) return "Gtick";
    if (unit == TPB_UNIT_TTICK) return "Ttick";
    if (unit == TPB_UNIT_PTICK) return "Ptick";
    if (unit == TPB_UNIT_ETICK) return "Etick";
    if (unit == TPB_UNIT_ZTICK) return "Ztick";
    if (unit == TPB_UNIT_YTICK) return "Ytick";
    
    if (unit == TPB_UNIT_TIMER) return "timer_unit";

    /* DATETIME units */
    if (unit == TPB_UNIT_SEC) return "sec";
    if (unit == TPB_UNIT_MIN) return "min";
    if (unit == TPB_UNIT_HOU) return "hour";
    if (unit == TPB_UNIT_DAY) return "day";
    if (unit == TPB_UNIT_MON) return "month";
    if (unit == TPB_UNIT_YEA) return "year";

    /* OP units */
    if (unit == TPB_UNIT_OP) return "op";
    if (unit == TPB_UNIT_KOP) return "Kop";
    if (unit == TPB_UNIT_MOP) return "Mop";
    if (unit == TPB_UNIT_GOP) return "Gop";
    if (unit == TPB_UNIT_TOP) return "Top";
    if (unit == TPB_UNIT_POP) return "Pop";
    if (unit == TPB_UNIT_EOP) return "Eop";
    if (unit == TPB_UNIT_ZOP) return "Zop";
    if (unit == TPB_UNIT_YOP) return "Yop";

    /* OPS units */
    if (unit == TPB_UNIT_OPS) return "op/s";
    if (unit == TPB_UNIT_KOPS) return "Kop/s";
    if (unit == TPB_UNIT_MOPS) return "Mop/s";
    if (unit == TPB_UNIT_GOPS) return "Gop/s";
    if (unit == TPB_UNIT_TOPS) return "Top/s";
    if (unit == TPB_UNIT_POPS) return "Pop/s";
    if (unit == TPB_UNIT_EOPS) return "Eop/s";
    if (unit == TPB_UNIT_ZOPS) return "Zop/s";
    if (unit == TPB_UNIT_YOPS) return "Yop/s";

    /* FLOPS units */
    if (unit == TPB_UNIT_FLOPS) return "FLOP/s";
    if (unit == TPB_UNIT_KFLOPS) return "KFLOP/s";
    if (unit == TPB_UNIT_MFLOPS) return "MFLOP/s";
    if (unit == TPB_UNIT_GFLOPS) return "GFLOP/s";
    if (unit == TPB_UNIT_TFLOPS) return "TFLOP/s";
    if (unit == TPB_UNIT_PFLOPS) return "PFLOP/s";
    if (unit == TPB_UNIT_EFLOPS) return "EFLOP/s";
    if (unit == TPB_UNIT_ZFLOPS) return "ZFLOP/s";
    if (unit == TPB_UNIT_YFLOPS) return "YFLOP/s";

    /* TOKENPS units */
    if (unit == TPB_UNIT_TOKENPS) return "token/s";
    if (unit == TPB_UNIT_KTOKENPS) return "Ktoken/s";
    if (unit == TPB_UNIT_MTOKENPS) return "Mtoken/s";
    if (unit == TPB_UNIT_GTOKENPS) return "Gtoken/s";
    if (unit == TPB_UNIT_TTOKENPS) return "Ttoken/s";
    if (unit == TPB_UNIT_PTOKENPS) return "Ptoken/s";
    if (unit == TPB_UNIT_ETOKENPS) return "Etoken/s";
    if (unit == TPB_UNIT_ZTOKENPS) return "Ztoken/s";
    if (unit == TPB_UNIT_YTOKENPS) return "Ytoken/s";

    /* TPS units */
    if (unit == TPB_UNIT_TPS) return "tps";
    if (unit == TPB_UNIT_KTPS) return "Ktps";
    if (unit == TPB_UNIT_MTPS) return "Mtps";
    if (unit == TPB_UNIT_GTPS) return "Gtps";
    if (unit == TPB_UNIT_TTPS) return "Ttps";
    if (unit == TPB_UNIT_PTPS) return "Ptps";
    if (unit == TPB_UNIT_ETPS) return "Etps";
    if (unit == TPB_UNIT_ZTPS) return "Ztps";
    if (unit == TPB_UNIT_YTPS) return "Ytps";

    /* Bit per second units */
    if (unit == TPB_UNIT_BITPS) return "bit/s";
    if (unit == TPB_UNIT_BYTEPS) return "B/s";
    if (unit == TPB_UNIT_KIBPS) return "KiB/s";
    if (unit == TPB_UNIT_MIBPS) return "MiB/s";
    if (unit == TPB_UNIT_GIBPS) return "GiB/s";
    if (unit == TPB_UNIT_TIBPS) return "TiB/s";
    if (unit == TPB_UNIT_PIBPS) return "PiB/s";
    if (unit == TPB_UNIT_EIBPS) return "EiB/s";
    if (unit == TPB_UNIT_ZIBPS) return "ZiB/s";
    if (unit == TPB_UNIT_YIBPS) return "YiB/s";

    /* Byte per second units */
    if (unit == TPB_UNIT_BPS) return "B/s";
    if (unit == TPB_UNIT_KBPS) return "KB/s";
    if (unit == TPB_UNIT_MBPS) return "MB/s";
    if (unit == TPB_UNIT_GBPS) return "GB/s";
    if (unit == TPB_UNIT_TBPS) return "TB/s";
    if (unit == TPB_UNIT_PBPS) return "PB/s";
    if (unit == TPB_UNIT_EBPS) return "EB/s";
    if (unit == TPB_UNIT_ZBPS) return "ZB/s";
    if (unit == TPB_UNIT_YBPS) return "YB/s";

    /* Bit per second units */
    if (unit == TPB_UNIT_BITPCY) return "bit/cy";
    if (unit == TPB_UNIT_BYTEPCY) return "B/cy";
    if (unit == TPB_UNIT_KIBPCY) return "KiB/cy";
    if (unit == TPB_UNIT_MIBPCY) return "MiB/cy";
    if (unit == TPB_UNIT_GIBPCY) return "GiB/cy";
    if (unit == TPB_UNIT_TIBPCY) return "TiB/cy";
    if (unit == TPB_UNIT_PIBPCY) return "PiB/cy";
    if (unit == TPB_UNIT_EIBPCY) return "EiB/cy";
    if (unit == TPB_UNIT_ZIBPCY) return "ZiB/cy";
    if (unit == TPB_UNIT_YIBPCY) return "YiB/cy";

    /* Byte per second units */
    if (unit == TPB_UNIT_BPCY) return "B/cy";
    if (unit == TPB_UNIT_KBPCY) return "KB/cy";
    if (unit == TPB_UNIT_MBPCY) return "MB/cy";
    if (unit == TPB_UNIT_GBPCY) return "GB/cy";
    if (unit == TPB_UNIT_TBPCY) return "TB/cy";
    if (unit == TPB_UNIT_PBPCY) return "PB/cy";
    if (unit == TPB_UNIT_EBPCY) return "EB/cy";
    if (unit == TPB_UNIT_ZBPCY) return "ZB/cy";
    if (unit == TPB_UNIT_YBPCY) return "YB/cy";

    /* Bit per second units */
    if (unit == TPB_UNIT_BITPTICK) return "bit/tick";
    if (unit == TPB_UNIT_BYTEPTICK) return "B/tick";
    if (unit == TPB_UNIT_KIBPTICK) return "KiB/tick";
    if (unit == TPB_UNIT_MIBPTICK) return "MiB/tick";
    if (unit == TPB_UNIT_GIBPTICK) return "GiB/tick";
    if (unit == TPB_UNIT_TIBPTICK) return "TiB/tick";
    if (unit == TPB_UNIT_PIBPTICK) return "PiB/tick";
    if (unit == TPB_UNIT_EIBPTICK) return "EiB/tick";
    if (unit == TPB_UNIT_ZIBPTICK) return "ZiB/tick";
    if (unit == TPB_UNIT_YIBPTICK) return "YiB/tick";

    /* Byte per second units */
    if (unit == TPB_UNIT_BPTICK) return "B/tick";
    if (unit == TPB_UNIT_KBPTICK) return "KB/tick";
    if (unit == TPB_UNIT_MBPTICK) return "MB/tick";
    if (unit == TPB_UNIT_GBPTICK) return "GB/tick";
    if (unit == TPB_UNIT_TBPTICK) return "TB/tick";
    if (unit == TPB_UNIT_PBPTICK) return "PB/tick";
    if (unit == TPB_UNIT_EBPTICK) return "EB/tick";
    if (unit == TPB_UNIT_ZBPTICK) return "ZB/tick";
    if (unit == TPB_UNIT_YBPTICK) return "YB/tick";

    return "unknown";
}

