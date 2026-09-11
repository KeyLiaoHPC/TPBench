/**
 * @file tpb_npb_class.h
 * @brief Runtime NPB CLASS tables (from sys/setparams.c).
 */

#ifndef TPB_NPB_CLASS_H
#define TPB_NPB_CLASS_H

extern char tpb_npb_c_class;
extern int tpb_npb_is_total_keys_log2;
extern int tpb_npb_is_max_key_log2;
extern int tpb_npb_is_num_buckets_log2;
extern int tpb_npb_is_min_procs;
extern int tpb_npb_is_max_procs;
extern int tpb_npb_dt_num_samples;
extern int tpb_npb_dt_std_deviation;
extern int tpb_npb_dt_num_sources;

int tpb_npb_class_allowed(const char *bench, char cls);
int tpb_npb_apply_class(const char *bench, char cls);
int tpb_npb_check_nprocs(const char *bench, int nprocs, char dt_graph);

#endif
