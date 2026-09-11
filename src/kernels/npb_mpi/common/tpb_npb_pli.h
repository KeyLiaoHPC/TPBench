/**
 * @file tpb_npb_pli.h
 * @brief Shared PLI register / entry / MPI capsule helpers for NPB-MPI.
 */

#ifndef TPB_NPB_PLI_H
#define TPB_NPB_PLI_H

#include "tpbench.h"

typedef int (*tpb_npb_run_fn)(char graph);

int tpb_npb_pli_register(const char *kname, const char *note,
                         const char *class_list, int has_graph);
int tpb_npb_pli_entry(int argc, char **argv, const char *kname,
                      const char *bench, int (*reg_fn)(void),
                      tpb_npb_run_fn run_fn, int has_graph);

#endif
