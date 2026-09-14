/**
 * @file tpb_npb_timer_meta.h
 * @brief Per-benchmark NPB timing-point names and notes.
 */

#ifndef TPB_NPB_TIMER_META_H
#define TPB_NPB_TIMER_META_H

typedef struct tpb_npb_timer_meta {
    int slot;           /**< Vendor timer slot (Fortran 1-based, IS 0-based) */
    int derived;        /**< 1 if this row is a derived totcomp/totcomm slot */
    const char *name;   /**< Timing-point name used as the output header */
    const char *note;   /**< Section, owning func/module, NPB aggregation */
} tpb_npb_timer_meta_t;

const tpb_npb_timer_meta_t *tpb_npb_timer_meta(const char *bench,
                                               int *count);
int tpb_npb_timer_total_slot(const char *bench);
int tpb_npb_timer_slot_base(const char *bench);

#endif /* TPB_NPB_TIMER_META_H */
