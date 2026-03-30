/*
 * test_capsule.c
 * Test pack A6: task capsule record and related APIs.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "include/tpb-public.h"
#include "corelib/rafdb/tpb-raf-types.h"
#include "corelib/tpb-autorecord.h"

typedef struct {
    const char *id;
    const char *name;
    int (*func)(void);
} test_case_t;

static char g_test_dir[512];

static int
run_pack(const char *pack, test_case_t *cases, int n, const char *filter)
{
    int pass = 0, fail = 0, run = 0;
    int i;

    if (filter == NULL) {
        printf("Running test pack %s (%d cases)\n", pack, n);
    }
    printf("------------------------------------------------------\n");

    for (i = 0; i < n; i++) {
        if (filter != NULL && strcmp(filter, cases[i].id) != 0) {
            continue;
        }
        run++;
        if (cases[i].func() == 0) {
            printf("[%s] %-40s PASS\n", cases[i].id, cases[i].name);
            pass++;
        } else {
            printf("[%s] %-40s FAIL\n", cases[i].id, cases[i].name);
            fail++;
        }
    }

    if (filter != NULL && run == 0) {
        fprintf(stderr, "No case matching '%s' in pack %s\n", filter, pack);
        return 1;
    }

    printf("------------------------------------------------------\n");
    if (filter == NULL) {
        printf("Pack %s: %d passed, %d failed\n\n", pack, pass, fail);
    }
    return fail;
}

static int
write_min_task(unsigned char *task_id_out)
{
    tpb_k_rthdl_t hdl;

    memset(&hdl, 0, sizeof(hdl));
    return tpb_record_write_task(&hdl, 0, task_id_out);
}

/* Separate wall/boot clock samples so TaskRecordIDs do not collide */
static void
sleep_task_sep(void)
{
    usleep(2000);
}

static void
setup_env_workspace(void)
{
    snprintf(g_test_dir, sizeof(g_test_dir),
             "/tmp/tpb_capsule_test_%d", (int)getpid());
    mkdir(g_test_dir, 0755);
    setenv("TPB_WORKSPACE", g_test_dir, 1);
    setenv("TPB_TBATCH_ID",
           "a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2", 1);
    setenv("TPB_KERNEL_ID",
           "f1e2d3c4b5a6f1e2d3c4b5a6f1e2d3c4b5a6f1e2", 1);
    setenv("TPB_HANDLE_INDEX", "0", 1);
}

static void
cleanup_env_workspace(void)
{
    char cmd[600];

    snprintf(cmd, sizeof(cmd), "rm -rf %s", g_test_dir);
    system(cmd);
    unsetenv("TPB_WORKSPACE");
    unsetenv("TPB_TBATCH_ID");
    unsetenv("TPB_KERNEL_ID");
    unsetenv("TPB_HANDLE_INDEX");
}

static int
read_self_exe(char *buf, size_t buflen)
{
    ssize_t n;

    n = readlink("/proc/self/exe", buf, buflen - 1);
    if (n < 0) {
        return -1;
    }
    buf[n] = '\0';
    return 0;
}

static void
unlink_test_capsule_shm(void)
{
    unsigned char kid[20];

    if (tpb_raf_hex_to_id(
            "f1e2d3c4b5a6f1e2d3c4b5a6f1e2d3c4b5a6f1e2", kid) == 0) {
        (void)tpb_k_unlink_capsule_sync_shm(kid, 0);
    }
}

/* A6.1 */
static int
test_id_taskcapsule_differs(void)
{
    unsigned char tb[20], k[20], t1[20], t2[20];
    int err;

    memset(tb, 0xab, 20);
    memset(k, 0xcd, 20);
    err = tpb_raf_gen_task_id(1, 2, "h", "u", tb, k, 0, 1, 2, t1);
    if (err != 0) {
        return 1;
    }
    err = tpb_raf_gen_taskcapsule_id(1, 2, "h", "u", tb, k, 0, 1, 2, t2);
    if (err != 0) {
        return 1;
    }
    if (memcmp(t1, t2, 20) == 0) {
        return 1;
    }
    return 0;
}

/* A6.2 */
static int
test_write_task_id_out(void)
{
    unsigned char tid[20];
    int err;
    size_t j;
    int allz = 1;

    setup_env_workspace();
    err = tpb_raf_init_workspace(g_test_dir);
    if (err != 0) {
        cleanup_env_workspace();
        return 1;
    }

    memset(tid, 0, sizeof(tid));
    err = write_min_task(tid);
    if (err != 0) {
        cleanup_env_workspace();
        return 1;
    }
    for (j = 0; j < 20; j++) {
        if (tid[j] != 0) {
            allz = 0;
            break;
        }
    }
    /* Expect non-zero TaskRecordID bytes */
    if (allz != 0) {
        cleanup_env_workspace();
        return 1;
    }
    cleanup_env_workspace();
    return 0;
}

/* A6.3 */
static int
test_write_task_null_out(void)
{
    task_entry_t *entries = NULL;
    int n = 0;
    int err;

    setup_env_workspace();
    err = tpb_raf_init_workspace(g_test_dir);
    if (err != 0) {
        cleanup_env_workspace();
        return 1;
    }

    err = tpb_raf_entry_list_task(g_test_dir, &entries, &n);
    free(entries);
    entries = NULL;
    if (err != 0 || n != 0) {
        cleanup_env_workspace();
        return 1;
    }

    err = write_min_task(NULL);
    if (err != 0) {
        cleanup_env_workspace();
        return 1;
    }

    err = tpb_raf_entry_list_task(g_test_dir, &entries, &n);
    if (err != 0 || n != 1) {
        free(entries);
        cleanup_env_workspace();
        return 1;
    }
    free(entries);
    cleanup_env_workspace();
    return 0;
}

/* A6.4 + A6.5 */
static int
test_create_capsule_header(void)
{
    unsigned char tid[20], cap[20];
    task_attr_t rattr;
    void *data = NULL;
    uint64_t ds = 0;
    int err;

    setup_env_workspace();
    err = tpb_raf_init_workspace(g_test_dir);
    if (err != 0) {
        cleanup_env_workspace();
        return 1;
    }

    err = write_min_task(tid);
    if (err != 0) {
        cleanup_env_workspace();
        return 1;
    }

    err = tpb_k_create_capsule_task(tid, cap);
    unlink_test_capsule_shm();
    if (err != 0) {
        cleanup_env_workspace();
        return 1;
    }

    memset(&rattr, 0, sizeof(rattr));
    err = tpb_raf_record_read_task(g_test_dir, cap, &rattr, &data, &ds);
    if (err != 0) {
        cleanup_env_workspace();
        return 1;
    }
    if (rattr.nheader != 1 || ds != 20U) {
        tpb_raf_free_headers(rattr.headers, rattr.nheader);
        free(data);
        cleanup_env_workspace();
        return 1;
    }
    if (strncmp(rattr.headers[0].name, "TPBLINK::TaskID",
                sizeof(rattr.headers[0].name)) != 0) {
        tpb_raf_free_headers(rattr.headers, rattr.nheader);
        free(data);
        cleanup_env_workspace();
        return 1;
    }
    if (rattr.headers[0].dimsizes[0] != 1U) {
        tpb_raf_free_headers(rattr.headers, rattr.nheader);
        free(data);
        cleanup_env_workspace();
        return 1;
    }
    if (memcmp(data, tid, 20) != 0) {
        tpb_raf_free_headers(rattr.headers, rattr.nheader);
        free(data);
        cleanup_env_workspace();
        return 1;
    }
    tpb_raf_free_headers(rattr.headers, rattr.nheader);
    free(data);
    cleanup_env_workspace();
    return 0;
}

/* A6.5 */
static int
test_tpbe_lists_capsule(void)
{
    unsigned char tid[20], cap[20];
    task_entry_t *entries = NULL;
    int n = 0;
    int err;
    int found = 0;
    int i;

    setup_env_workspace();
    err = tpb_raf_init_workspace(g_test_dir);
    if (err != 0) {
        cleanup_env_workspace();
        return 1;
    }

    err = write_min_task(tid);
    if (err != 0) {
        cleanup_env_workspace();
        return 1;
    }

    err = tpb_k_create_capsule_task(tid, cap);
    unlink_test_capsule_shm();
    if (err != 0) {
        cleanup_env_workspace();
        return 1;
    }

    err = tpb_raf_entry_list_task(g_test_dir, &entries, &n);
    if (err != 0 || n < 2) {
        free(entries);
        cleanup_env_workspace();
        return 1;
    }
    for (i = 0; i < n; i++) {
        if (memcmp(entries[i].task_record_id, cap, 20) == 0) {
            found = 1;
            break;
        }
    }
    free(entries);
    if (found == 0) {
        cleanup_env_workspace();
        return 1;
    }
    cleanup_env_workspace();
    return 0;
}

/* A6.6 */
static int
test_append_once(void)
{
    unsigned char tid0[20], tid1[20], cap[20];
    task_attr_t rattr;
    void *data = NULL;
    uint64_t ds = 0;
    int err;

    setup_env_workspace();
    err = tpb_raf_init_workspace(g_test_dir);
    if (err != 0) {
        cleanup_env_workspace();
        return 1;
    }

    err = write_min_task(tid0);
    if (err != 0) {
        cleanup_env_workspace();
        return 1;
    }
    sleep_task_sep();
    err = write_min_task(tid1);
    if (err != 0) {
        cleanup_env_workspace();
        return 1;
    }

    err = tpb_k_create_capsule_task(tid0, cap);
    unlink_test_capsule_shm();
    if (err != 0) {
        cleanup_env_workspace();
        return 1;
    }

    err = tpb_k_append_capsule_task(cap, tid1);
    if (err != 0) {
        cleanup_env_workspace();
        return 1;
    }

    memset(&rattr, 0, sizeof(rattr));
    err = tpb_raf_record_read_task(g_test_dir, cap, &rattr, &data, &ds);
    if (err != 0) {
        cleanup_env_workspace();
        return 1;
    }
    if (ds != 40U || rattr.headers[0].dimsizes[0] != 2U) {
        tpb_raf_free_headers(rattr.headers, rattr.nheader);
        free(data);
        cleanup_env_workspace();
        return 1;
    }
    if (memcmp(data, tid0, 20) != 0 || memcmp((unsigned char *)data + 20,
                                                tid1, 20) != 0) {
        tpb_raf_free_headers(rattr.headers, rattr.nheader);
        free(data);
        cleanup_env_workspace();
        return 1;
    }
    tpb_raf_free_headers(rattr.headers, rattr.nheader);
    free(data);
    cleanup_env_workspace();
    return 0;
}

/* A6.7 */
static int
test_append_multi(void)
{
    unsigned char tid0[20], cap[20], extra[3][20];
    int err;
    int k;

    setup_env_workspace();
    err = tpb_raf_init_workspace(g_test_dir);
    if (err != 0) {
        cleanup_env_workspace();
        return 1;
    }

    err = write_min_task(tid0);
    if (err != 0) {
        cleanup_env_workspace();
        return 1;
    }

    for (k = 0; k < 3; k++) {
        sleep_task_sep();
        err = write_min_task(extra[k]);
        if (err != 0) {
            cleanup_env_workspace();
            return 1;
        }
    }

    err = tpb_k_create_capsule_task(tid0, cap);
    unlink_test_capsule_shm();
    if (err != 0) {
        cleanup_env_workspace();
        return 1;
    }

    for (k = 0; k < 3; k++) {
        err = tpb_k_append_capsule_task(cap, extra[k]);
        if (err != 0) {
            cleanup_env_workspace();
            return 1;
        }
    }

    {
        task_attr_t rattr;
        void *data = NULL;
        uint64_t ds = 0;
        unsigned char *p;
        int j;

        memset(&rattr, 0, sizeof(rattr));
        err = tpb_raf_record_read_task(g_test_dir, cap, &rattr, &data, &ds);
        if (err != 0) {
            cleanup_env_workspace();
            return 1;
        }
        if (ds != 80U || rattr.headers[0].dimsizes[0] != 4U) {
            tpb_raf_free_headers(rattr.headers, rattr.nheader);
            free(data);
            cleanup_env_workspace();
            return 1;
        }
        p = (unsigned char *)data;
        if (memcmp(p, tid0, 20) != 0) {
            tpb_raf_free_headers(rattr.headers, rattr.nheader);
            free(data);
            cleanup_env_workspace();
            return 1;
        }
        for (j = 0; j < 3; j++) {
            if (memcmp(p + 20 * (size_t)(j + 1), extra[j], 20) != 0) {
                tpb_raf_free_headers(rattr.headers, rattr.nheader);
                free(data);
                cleanup_env_workspace();
                return 1;
            }
        }
        tpb_raf_free_headers(rattr.headers, rattr.nheader);
        free(data);
    }

    cleanup_env_workspace();
    return 0;
}

/*
 * A6.8: two fresh processes each run write+append (exec), exercising
 * capsule locking across PIDs without fork-after-corelib.
 */
static int
test_subprocess_workers_append(void)
{
    unsigned char tid0[20], cap[20], extra[2][20];
    char caphex[41], tidhex[2][41];
    char exepath[512];
    char wsbuf[512];
    char cmd[2048];
    int err;
    int k;
    int rc;

    setup_env_workspace();
    err = tpb_raf_init_workspace(g_test_dir);
    if (err != 0) {
        cleanup_env_workspace();
        return 1;
    }

    err = write_min_task(tid0);
    if (err != 0) {
        cleanup_env_workspace();
        return 1;
    }

    err = tpb_k_create_capsule_task(tid0, cap);
    unlink_test_capsule_shm();
    if (err != 0) {
        cleanup_env_workspace();
        return 1;
    }

    for (k = 0; k < 2; k++) {
        sleep_task_sep();
        err = write_min_task(extra[k]);
        if (err != 0) {
            cleanup_env_workspace();
            return 1;
        }
    }

    if (read_self_exe(exepath, sizeof(exepath)) != 0) {
        cleanup_env_workspace();
        return 1;
    }

    tpb_raf_id_to_hex(cap, caphex);
    snprintf(wsbuf, sizeof(wsbuf), "%s", g_test_dir);

    for (k = 0; k < 2; k++) {
        tpb_raf_id_to_hex(extra[k], tidhex[k]);
        snprintf(cmd, sizeof(cmd),
                 "env TPB_WORKSPACE='%s' "
                 "TPB_TBATCH_ID=a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2 "
                 "TPB_KERNEL_ID=f1e2d3c4b5a6f1e2d3c4b5a6f1e2d3c4b5a6f1e2 "
                 "TPB_HANDLE_INDEX=0 "
                 "'%s' --capsule-append %s %s",
                 wsbuf, exepath, caphex, tidhex[k]);
        rc = system(cmd);
        if (rc == -1 || !WIFEXITED(rc) || WEXITSTATUS(rc) != 0) {
            cleanup_env_workspace();
            return 1;
        }
    }

    {
        task_attr_t rattr;
        void *data = NULL;
        uint64_t ds = 0;

        memset(&rattr, 0, sizeof(rattr));
        err = tpb_raf_record_read_task(g_test_dir, cap, &rattr, &data, &ds);
        if (err != 0) {
            cleanup_env_workspace();
            return 1;
        }
        if (ds != 60U || rattr.headers[0].dimsizes[0] != 3U) {
            tpb_raf_free_headers(rattr.headers, rattr.nheader);
            free(data);
            cleanup_env_workspace();
            return 1;
        }
        if (memcmp(data, tid0, 20) != 0
            || memcmp((unsigned char *)data + 20, extra[0], 20) != 0
            || memcmp((unsigned char *)data + 40, extra[1], 20) != 0) {
            tpb_raf_free_headers(rattr.headers, rattr.nheader);
            free(data);
            cleanup_env_workspace();
            return 1;
        }
        tpb_raf_free_headers(rattr.headers, rattr.nheader);
        free(data);
    }

    cleanup_env_workspace();
    return 0;
}

static int
capsule_append_submain(int argc, char **argv)
{
    unsigned char cap[20], tid[20];

    if (argc < 4) {
        return 1;
    }
    if (tpb_raf_hex_to_id(argv[2], cap) != 0) {
        return 1;
    }
    if (tpb_raf_hex_to_id(argv[3], tid) != 0) {
        return 1;
    }
    if (tpb_k_append_capsule_task(cap, tid) != 0) {
        return 1;
    }
    return 0;
}

int
main(int argc, char **argv)
{
    const char *filter;
    int fail;

    if (argc >= 4 && strcmp(argv[1], "--capsule-append") == 0) {
        return capsule_append_submain(argc, argv);
    }

    filter = (argc > 1) ? argv[1] : NULL;
    test_case_t cases[] = {
        { "A6.1", "task vs taskcapsule id", test_id_taskcapsule_differs },
        { "A6.2", "write_task id_out", test_write_task_id_out },
        { "A6.3", "write_task NULL id_out", test_write_task_null_out },
        { "A6.4", "create capsule + header", test_create_capsule_header },
        { "A6.5", "task.tpbe lists capsule", test_tpbe_lists_capsule },
        { "A6.6", "append once", test_append_once },
        { "A6.7", "append multi", test_append_multi },
        { "A6.8", "subprocess append", test_subprocess_workers_append },
    };

    fail = run_pack("A6", cases, (int)(sizeof(cases) / sizeof(cases[0])),
                    filter);
    return (fail > 0) ? 1 : 0;
}
