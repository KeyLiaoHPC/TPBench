/*
 * test-cli-task-fixture.h
 * Shared workspace seeding and tpbcli helpers for Pack B7 task CLI tests.
 */

#ifndef TEST_CLI_TASK_FIXTURE_H
#define TEST_CLI_TASK_FIXTURE_H

#include <stddef.h>

#ifndef TPB_TEST_TPBCLI_STR
#define TPB_TEST_TPBCLI_STR "./bin/tpbcli"
#endif

/** PID-scoped workspace path (valid after setup). */
extern char tpb_test_task_ws[512];

void tpb_test_task_fixture_setup(void);
void tpb_test_task_fixture_cleanup(void);

/** Set TPB_WORKSPACE / TBATCH / KERNEL / HANDLE env for autorecord APIs. */
void tpb_test_task_fixture_seed_env(void);

/** Write one standalone task (derive_to == 0). Returns 0 on success. */
int tpb_test_task_write_standalone(int exit_code,
                                   unsigned char task_id_out[20]);

/**
 * Create a capsule with @p n_members total TaskID rows (first task + appends).
 * Sets derive_to on all member tasks to the capsule id.
 * @p cap_id_out receives the capsule TaskRecordID.
 */
int tpb_test_task_seed_capsule(int n_members, unsigned char cap_id_out[20]);

/** Register kernel name for TPB_KERNEL_ID in the workspace index. */
int tpb_test_task_seed_kernel_name(const char *kernel_name);

void tpb_test_task_sleep_sep(void);

/** Write standalone task with one double output metric (derive_to may be zero). */
int tpb_test_task_write_metric_task(const unsigned char task_id[20],
                                    const unsigned char derive_to[20],
                                    const char *metric_name,
                                    const double *vals,
                                    int nval);

/**
 * @p task_args is everything after `task` (e.g. `ls -n 3`).
 * @return child exit status, or -1 on popen failure.
 */
int tpb_test_task_run_cmd(const char *task_args, char *outbuf, size_t outbuf_sz);

int tpb_test_task_ridmap_path(char *out, size_t outlen);

/** Read RIDMAP hex lines; caller frees with tpb_test_task_ridmap_free(). */
int tpb_test_task_ridmap_read(unsigned char (**ids_out)[20], int *nids_out);
void tpb_test_task_ridmap_free(unsigned char (*ids)[20]);

#endif /* TEST_CLI_TASK_FIXTURE_H */
