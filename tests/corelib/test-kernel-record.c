/*
 * test-kernel-record.c
 * Pack A7: kernel record writes registered parms/metrics and metadata headers.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "include/tpb-public.h"
#include "corelib/rafdb/tpb-raf-kernel-meta.h"

#ifndef TPB_TEST_TPB_HOME
#define TPB_TEST_TPB_HOME ""
#endif

static char g_test_dir[512];

static void
setup_test_dir(void)
{
    snprintf(g_test_dir, sizeof(g_test_dir),
             "/tmp/tpb_kernrec_test_%d", (int)getpid());
    tpb_raf_init_workspace(g_test_dir);
}

static void
cleanup_test_dir(void)
{
    char cmd[600];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", g_test_dir);
    system(cmd);
}

static int
test_build_attr_counts(void)
{
    tpb_kernel_t k;
    tpb_rt_parm_t parms[2];
    tpb_k_output_t outs[1];
    kernel_attr_t attr;
    void *data = NULL;
    uint64_t datasize = 0;
    unsigned char kid[20];
    int err;

    memset(&k, 0, sizeof(k));
    memset(kid, 0xAB, 20);
    snprintf(k.info.name, sizeof(k.info.name), "%s", "demo");
    snprintf(k.info.note, sizeof(k.info.note), "%s", "demo kernel");
    k.info.kctrl = TPB_KTYPE_PLI;
    k.info.nparms = 2;
    k.info.parms = parms;
    k.info.nouts = 1;
    k.info.outs = outs;

    memset(parms, 0, sizeof(parms));
    snprintf(parms[0].name, sizeof(parms[0].name), "%s", "ntest");
    parms[0].ctrlbits = TPB_PARM_CLI | TPB_INT64_T;
    parms[0].value.i64 = 10;
    snprintf(parms[1].name, sizeof(parms[1].name), "%s", "size");
    parms[1].ctrlbits = TPB_PARM_CLI | TPB_UINT32_T;
    parms[1].value.u64 = 1024;

    memset(outs, 0, sizeof(outs));
    snprintf(outs[0].name, sizeof(outs[0].name), "%s", "bandwidth");
    outs[0].dtype = TPB_DOUBLE_T;
    outs[0].unit = TPB_UNIT_MBPS;

    err = tpb_raf_kernel_build_registered_attr(&k, kid, &attr, &data, &datasize);
    if (err != TPBE_SUCCESS) {
        return 1;
    }
    if (attr.nparm != 2 || attr.nmetric != 1 ||
        attr.nheader != 2 + 1 + TPB_RAF_KERNEL_META_HDR_COUNT) {
        tpb_raf_kernel_free_built_attr(&attr, data);
        return 1;
    }
    if (strcmp(attr.headers[0].name, "ntest") != 0 ||
        strcmp(attr.headers[2].name, "bandwidth") != 0 ||
        strcmp(attr.headers[5].name, TPB_RAF_KERNEL_HDR_DEPENDENCY) != 0) {
        tpb_raf_kernel_free_built_attr(&attr, data);
        return 1;
    }

    tpb_raf_kernel_free_built_attr(&attr, data);
    return 0;
}

static int
test_stream_register_record(void)
{
#ifndef TPB_TEST_STREAM_SO
    return 0;
#else
    char workspace[512];
    const char *names[] = { "stream" };
    tpb_kernel_t *kernel = NULL;
    kernel_entry_t *entries = NULL;
    int n = 0;
    kernel_attr_t attr;
    void *data = NULL;
    uint64_t datasize = 0;
    int err;

    snprintf(workspace, sizeof(workspace), "%s", g_test_dir);
    setenv("TPB_WORKSPACE", workspace, 1);
    if (TPB_TEST_TPB_HOME[0] != '\0') {
        setenv("TPB_HOME", TPB_TEST_TPB_HOME, 1);
    }

    err = tpb_corelib_init(workspace);
    if (err != TPBE_SUCCESS) {
        return 1;
    }

    err = tpb_register_kernels(1, names);
    if (err != TPBE_SUCCESS) {
        return 1;
    }

    err = tpb_query_kernel(-1, "stream", &kernel);
    if (err <= 0 || kernel == NULL) {
        return 1;
    }

    err = tpb_raf_entry_list_kernel(workspace, &entries, &n);
    if (err != TPBE_SUCCESS || n < 1) {
        tpb_free_kernel(kernel);
        free(kernel);
        return 1;
    }

    memset(&attr, 0, sizeof(attr));
    err = tpb_raf_record_read_kernel(workspace, entries[0].kernel_id,
                                     &attr, &data, &datasize);
    if (err != TPBE_SUCCESS) {
        free(entries);
        tpb_free_kernel(kernel);
        free(kernel);
        return 1;
    }

    if (attr.nparm < 3 || attr.nmetric < 4 ||
        attr.nheader != attr.nparm + attr.nmetric + TPB_RAF_KERNEL_META_HDR_COUNT ||
        attr.utc_bits == 0 || entries[0].utc_bits == 0 ||
        entries[0].utc_bits != attr.utc_bits) {
        tpb_raf_free_headers(attr.headers, attr.nheader);
        free(data);
        free(entries);
        tpb_free_kernel(kernel);
        free(kernel);
        return 1;
    }

    tpb_raf_free_headers(attr.headers, attr.nheader);
    free(data);
    free(entries);
    tpb_free_kernel(kernel);
    free(kernel);
    return 0;
#endif
}

int
main(int argc, char **argv)
{
    const char *id = (argc >= 2) ? argv[1] : "";
    int rc = 0;

    setup_test_dir();

    if (strcmp(id, "A7.1") == 0 || id[0] == '\0') {
        rc = test_build_attr_counts();
        printf("[A7.1] build_attr_counts %s\n", rc == 0 ? "PASS" : "FAIL");
    }
    if (strcmp(id, "A7.2") == 0 || id[0] == '\0') {
        if (test_stream_register_record() != 0) {
            rc = 1;
        }
        printf("[A7.2] stream_register_record %s\n", rc == 0 ? "PASS" : "FAIL");
    }

    cleanup_test_dir();
    return rc;
}
