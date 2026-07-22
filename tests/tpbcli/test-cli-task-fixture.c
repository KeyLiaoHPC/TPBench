/*
 * test-cli-task-fixture.c
 */

#include "test-cli-task-fixture.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "tpb-public.h"

#define TPB_TEST_TASK_RIDMAP_NAME "tpb_rt_local_ridmap"

char tpb_test_task_ws[512];

static void
_sf_unlink_capsule_shm(void)
{
    unsigned char kid[20];

    if (tpb_raf_hex_to_id(
            "f1e2d3c4b5a6f1e2d3c4b5a6f1e2d3c4b5a6f1e2", kid) == 0) {
        (void)tpb_k_unlink_capsule_sync_shm(kid, 0);
    }
}

void
tpb_test_task_fixture_seed_env(void)
{
    setenv("TPB_WORKSPACE", tpb_test_task_ws, 1);
    setenv("TPB_TBATCH_ID",
           "a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2", 1);
    setenv("TPB_KERNEL_ID",
           "f1e2d3c4b5a6f1e2d3c4b5a6f1e2d3c4b5a6f1e2", 1);
    setenv("TPB_HANDLE_INDEX", "0", 1);
}

void
tpb_test_task_fixture_setup(void)
{
    snprintf(tpb_test_task_ws, sizeof(tpb_test_task_ws),
             "/tmp/tpb_task_cli_%d", (int)getpid());
    (void)tpb_test_task_fixture_seed_env();
    if (tpb_raf_init_workspace(tpb_test_task_ws) != TPBE_SUCCESS) {
        fprintf(stderr, "fixture: tpb_raf_init_workspace failed\n");
    }
}

void
tpb_test_task_fixture_cleanup(void)
{
    char cmd[640];

    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", tpb_test_task_ws);
    (void)system(cmd);
    unsetenv("TPB_WORKSPACE");
    unsetenv("TPB_TBATCH_ID");
    unsetenv("TPB_KERNEL_ID");
    unsetenv("TPB_HANDLE_INDEX");
    tpb_test_task_ws[0] = '\0';
}

void
tpb_test_task_sleep_sep(void)
{
    usleep(2000);
}

int
tpb_test_task_write_standalone(int exit_code, unsigned char task_id_out[20])
{
    tpb_k_rthdl_t hdl;

    memset(&hdl, 0, sizeof(hdl));
    return tpb_k_write_task(&hdl, exit_code, task_id_out);
}

int
tpb_test_task_seed_kernel_name(const char *kernel_name)
{
    kernel_entry_t ent;
    unsigned char kid[20];

    if (kernel_name == NULL) {
        return -1;
    }
    if (tpb_raf_hex_to_id(
            "f1e2d3c4b5a6f1e2d3c4b5a6f1e2d3c4b5a6f1e2", kid) != 0) {
        return -1;
    }
    memset(&ent, 0, sizeof(ent));
    memcpy(ent.kernel_id, kid, 20);
    snprintf(ent.kernel_name, sizeof(ent.kernel_name), "%s", kernel_name);
    if (tpb_raf_entry_append_kernel(tpb_test_task_ws, &ent) != TPBE_SUCCESS) {
        return -1;
    }
    return 0;
}

int
tpb_test_task_seed_capsule(int n_members, unsigned char cap_id_out[20])
{
    unsigned char tid0[20];
    unsigned char cap[20];
    unsigned char extra[16][20];
    int err;
    int k;

    if (n_members < 1 || n_members > 17 || cap_id_out == NULL) {
        return -1;
    }

    err = tpb_test_task_write_standalone(0, tid0);
    if (err != TPBE_SUCCESS) {
        return -1;
    }

    for (k = 0; k < n_members - 1; k++) {
        tpb_test_task_sleep_sep();
        err = tpb_test_task_write_standalone(0, extra[k]);
        if (err != TPBE_SUCCESS) {
            return -1;
        }
    }

    err = tpb_k_create_capsule_task(tid0, cap);
    if (err != TPBE_SUCCESS) {
        _sf_unlink_capsule_shm();
        return -1;
    }

    for (k = 0; k < n_members - 1; k++) {
        err = tpb_k_append_capsule_task(cap, extra[k]);
        if (err != TPBE_SUCCESS) {
            _sf_unlink_capsule_shm();
            return -1;
        }
        err = tpb_k_task_set_derive_to(extra[k], cap);
        if (err != TPBE_SUCCESS) {
            _sf_unlink_capsule_shm();
            return -1;
        }
    }

    err = tpb_k_task_set_derive_to(tid0, cap);
    _sf_unlink_capsule_shm();
    if (err != TPBE_SUCCESS) {
        return -1;
    }

    memcpy(cap_id_out, cap, 20);
    return 0;
}

int
tpb_test_task_run_cmd(const char *task_args, char *outbuf, size_t outbuf_sz)
{
    char cmd[16384];
    FILE *fp;

    if (task_args == NULL || tpb_test_task_ws[0] == '\0') {
        return -1;
    }
    if (outbuf != NULL && outbuf_sz > 0) {
        outbuf[0] = '\0';
    }
    snprintf(cmd, sizeof(cmd),
             "\"%s\" --workspace \"%s\" task %s 2>&1",
             TPB_TEST_TPBCLI_STR, tpb_test_task_ws, task_args);
    fp = popen(cmd, "r");
    if (fp == NULL) {
        return -1;
    }
    if (outbuf != NULL && outbuf_sz > 0) {
        size_t n = fread(outbuf, 1, outbuf_sz - 1, fp);
        outbuf[n] = '\0';
        while (fgetc(fp) != EOF) {
        }
    } else {
        while (fgetc(fp) != EOF) {
        }
    }
    {
        int st = pclose(fp);
        if (st == -1) {
            return -1;
        }
        return WEXITSTATUS(st);
    }
}

int
tpb_test_task_ridmap_path(char *out, size_t outlen)
{
    int n;

    if (out == NULL || outlen == 0 || tpb_test_task_ws[0] == '\0') {
        return -1;
    }
    n = snprintf(out, outlen, "%s/.tmp/%s", tpb_test_task_ws,
                 TPB_TEST_TASK_RIDMAP_NAME);
    if (n < 0 || (size_t)n >= outlen) {
        return -1;
    }
    return 0;
}

int
tpb_test_task_ridmap_read(unsigned char (**ids_out)[20], int *nids_out)
{
    char path[1024];
    FILE *fp;
    char line[128];
    unsigned char (*ids)[20] = NULL;
    int n = 0;
    int cap = 0;

    if (ids_out == NULL || nids_out == NULL) {
        return -1;
    }
    *ids_out = NULL;
    *nids_out = 0;
    if (tpb_test_task_ridmap_path(path, sizeof(path)) != 0) {
        return -1;
    }
    fp = fopen(path, "r");
    if (fp == NULL) {
        return 0;
    }
    while (fgets(line, sizeof(line), fp) != NULL) {
        char *p = line;
        size_t len;
        unsigned char id[20];
        unsigned char (*grown)[20];

        while (*p == ' ' || *p == '\t') {
            p++;
        }
        if (*p == '\0' || *p == '\n' || *p == '#') {
            continue;
        }
        len = strlen(p);
        while (len > 0 &&
               (p[len - 1] == '\n' || p[len - 1] == '\r' ||
                p[len - 1] == ' ' || p[len - 1] == '\t')) {
            p[--len] = '\0';
        }
        if (len != 40 || tpb_raf_hex_to_id(p, id) != 0) {
            fclose(fp);
            free(ids);
            return -1;
        }
        if (n == cap) {
            int ncap = (cap == 0) ? 16 : cap * 2;

            grown = (unsigned char (*)[20])realloc(
                ids, (size_t)ncap * sizeof(*grown));
            if (grown == NULL) {
                fclose(fp);
                free(ids);
                return -1;
            }
            ids = grown;
            cap = ncap;
        }
        memcpy(ids[n], id, 20);
        n++;
    }
    fclose(fp);
    *ids_out = ids;
    *nids_out = n;
    return 0;
}

void
tpb_test_task_ridmap_free(unsigned char (*ids)[20])
{
    free(ids);
}

int
tpb_test_task_write_metric_task(const unsigned char task_id[20],
                                const unsigned char derive_to[20],
                                const char *metric_name,
                                const double *vals,
                                int nval)
{
    task_attr_t attr;
    tpb_meta_header_t hdr;
    double *data;
    task_entry_t ent;
    int err;

    if (task_id == NULL || metric_name == NULL || vals == NULL || nval <= 0) {
        return -1;
    }
    memset(&attr, 0, sizeof(attr));
    memcpy(attr.task_record_id, task_id, 20);
    memcpy(attr.derive_to, derive_to, 20);
    memset(attr.inherit_from, 0xFF, 20);
    if (tpb_raf_hex_to_id(
            "a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2", attr.tbatch_id) != 0) {
        return -1;
    }
    if (tpb_raf_hex_to_id(
            "f1e2d3c4b5a6f1e2d3c4b5a6f1e2d3c4b5a6f1e2", attr.kernel_id) != 0) {
        return -1;
    }
    attr.ninput = 0;
    attr.noutput = 1;
    attr.nheader = 1;
    memset(&hdr, 0, sizeof(hdr));
    hdr.ndim = (nval > 1) ? 1 : 0;
    hdr.dimsizes[0] = (uint32_t)nval;
    hdr.type_bits = (uint32_t)(TPB_DOUBLE_T & TPB_PARM_TYPE_MASK);
    hdr.uattr_bits = TPB_UNIT_UNDEF;
    hdr.data_size = (uint64_t)nval * 8u;
    snprintf(hdr.name, sizeof(hdr.name), "%s", metric_name);
    hdr.block_size = 256;
    attr.headers = &hdr;
    data = (double *)malloc((size_t)nval * sizeof(*data));
    if (data == NULL) {
        return -1;
    }
    memcpy(data, vals, (size_t)nval * sizeof(*data));
    err = tpb_raf_record_write_task(tpb_test_task_ws, &attr, data,
                                    (uint64_t)nval * 8u);
    free(data);
    if (err != TPBE_SUCCESS) {
        return -1;
    }
    memset(&ent, 0, sizeof(ent));
    memcpy(ent.task_record_id, task_id, 20);
    memcpy(ent.derive_to, derive_to, 20);
    memcpy(ent.inherit_from, attr.inherit_from, 20);
    memcpy(ent.tbatch_id, attr.tbatch_id, 20);
    memcpy(ent.kernel_id, attr.kernel_id, 20);
    return tpb_raf_entry_append_task(tpb_test_task_ws, &ent);
}
