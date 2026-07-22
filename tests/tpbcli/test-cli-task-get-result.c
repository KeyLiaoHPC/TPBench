/*
 * test-cli-task-get-result.c
 * Pack B7: `tpbcli task get-result` selection and aggregation tests.
 */

#include <ctype.h>
#include <limits.h>
#ifdef __linux__
#include <linux/limits.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "tpb-public.h"
#include "tpbcli-task-internal.h"
#include "test-cli-task-fixture.h"

#ifndef TPB_TEST_TPBCLI_STR
#define TPB_TEST_TPBCLI_STR "./bin/tpbcli"
#endif

static char g_ws[512];
static int g_pass;
static int g_fail;

#define FAIL(msg) do { \
    g_fail++; \
    fprintf(stderr, "  FAIL: %s\n", (msg)); \
} while (0)

#define PASS() do { g_pass++; } while (0)

static void
setup_ws(void)
{
    snprintf(g_ws, sizeof(g_ws), "/tmp/tpb_gr_test_%d", (int)getpid());
    mkdir(g_ws, 0700);
    setenv("TPB_WORKSPACE", g_ws, 1);
    (void)tpb_raf_init_workspace(g_ws);
}

static void
cleanup_ws(void)
{
    char cmd[600];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", g_ws);
    system(cmd);
    unsetenv("TPB_WORKSPACE");
}

static tpb_meta_header_t
make_out_hdr(const char *name, TPB_DTYPE dt, uint64_t uattr, uint32_t nelem)
{
    tpb_meta_header_t h;

    memset(&h, 0, sizeof(h));
    h.ndim = (nelem > 1) ? 1 : 0;
    h.dimsizes[0] = nelem;
    h.type_bits = (uint32_t)(dt & TPB_PARM_TYPE_MASK);
    h.uattr_bits = uattr;
    h.data_size = nelem * 8;
    snprintf(h.name, sizeof(h.name), "%s", name);
    h.block_size = 256;
    return h;
}

static int
write_task_metric(const unsigned char id[20], const unsigned char derive[20],
                  const char *metric, const double *vals, int nval)
{
    task_attr_t attr;
    tpb_meta_header_t hdr;
    double *data;
    task_entry_t ent;
    int err;

    memset(&attr, 0, sizeof(attr));
    memcpy(attr.task_record_id, id, 20);
    memcpy(attr.derive_to, derive, 20);
    memset(attr.inherit_from, 0xFF, 20);
    memset(attr.tbatch_id, 0x11, 20);
    memset(attr.kernel_id, 0x22, 20);
    attr.ninput = 0;
    attr.noutput = 1;
    attr.nheader = 1;
    hdr = make_out_hdr(metric, TPB_DOUBLE_T, TPB_UNIT_UNDEF,
                       (uint32_t)nval);
    attr.headers = &hdr;
    data = (double *)malloc((size_t)nval * sizeof(*data));
    if (data == NULL) {
        return 1;
    }
    memcpy(data, vals, (size_t)nval * sizeof(*data));
    err = tpb_raf_record_write_task(g_ws, &attr, data, (uint64_t)nval * 8);
    free(data);
    if (err != 0) {
        return 1;
    }
    memset(&ent, 0, sizeof(ent));
    memcpy(ent.task_record_id, id, 20);
    memcpy(ent.derive_to, derive, 20);
    memcpy(ent.inherit_from, attr.inherit_from, 20);
    memcpy(ent.tbatch_id, attr.tbatch_id, 20);
    memcpy(ent.kernel_id, attr.kernel_id, 20);
    return tpb_raf_entry_append_task(g_ws, &ent);
}

/*
 * Write one member/standalone task record with an explicit headers array
 * (arbitrary noutput/nheader, dtype and uattr_bits per header) into the
 * `tpb_test_task_ws` fixture workspace, then append its task entry.
 */
static int
write_member_hdrs(const unsigned char id[20], const unsigned char derive[20],
                  tpb_meta_header_t *hdrs, int nheader, int noutput,
                  const void *data, uint64_t datasize)
{
    task_attr_t attr;
    task_entry_t ent;
    int err;

    memset(&attr, 0, sizeof(attr));
    memcpy(attr.task_record_id, id, 20);
    memcpy(attr.derive_to, derive, 20);
    memset(attr.inherit_from, 0xFF, 20);
    if (tpb_raf_hex_to_id("a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2",
                          attr.tbatch_id) != 0) {
        return 1;
    }
    if (tpb_raf_hex_to_id("f1e2d3c4b5a6f1e2d3c4b5a6f1e2d3c4b5a6f1e2",
                          attr.kernel_id) != 0) {
        return 1;
    }
    attr.ninput = 0;
    attr.noutput = (uint32_t)noutput;
    attr.nheader = (uint32_t)nheader;
    attr.headers = hdrs;
    err = tpb_raf_record_write_task(tpb_test_task_ws, &attr, data, datasize);
    if (err != 0) {
        return 1;
    }
    memset(&ent, 0, sizeof(ent));
    memcpy(ent.task_record_id, id, 20);
    memcpy(ent.derive_to, derive, 20);
    memcpy(ent.inherit_from, attr.inherit_from, 20);
    memcpy(ent.tbatch_id, attr.tbatch_id, 20);
    memcpy(ent.kernel_id, attr.kernel_id, 20);
    return tpb_raf_entry_append_task(tpb_test_task_ws, &ent);
}

/*
 * Write a capsule record whose `TaskID` link header points at @p member_ids,
 * with `derive_to` all-zero so it is a top-level entry, then append its
 * task entry, into the `tpb_test_task_ws` fixture workspace.
 */
static int
write_capsule_link(const unsigned char cap_id[20],
                   const unsigned char (*member_ids)[20], int nmember)
{
    task_attr_t cattr;
    tpb_meta_header_t ch;
    task_entry_t ent;
    unsigned char z[20] = {0};
    unsigned char *blob;
    int k;
    int err;

    memset(&cattr, 0, sizeof(cattr));
    memcpy(cattr.task_record_id, cap_id, 20);
    memset(cattr.derive_to, 0, 20);
    memset(cattr.inherit_from, 0xFF, 20);
    cattr.ninput = 0;
    cattr.noutput = 0;
    cattr.nheader = 1;
    ch = make_out_hdr("TaskID", TPB_UNSIGNED_CHAR_T, TPB_UNIT_UNDEF,
                      (uint32_t)nmember);
    snprintf(ch.tag, sizeof(ch.tag), "%s", TPB_TAG_LINK);
    ch.data_size = (uint64_t)nmember * 20;
    cattr.headers = &ch;

    blob = (unsigned char *)malloc((size_t)nmember * 20);
    if (blob == NULL) {
        return 1;
    }
    for (k = 0; k < nmember; k++) {
        memcpy(blob + k * 20, member_ids[k], 20);
    }
    err = tpb_raf_record_write_task(tpb_test_task_ws, &cattr, blob,
                                    (uint64_t)nmember * 20);
    free(blob);
    if (err != 0) {
        return 1;
    }
    memset(&ent, 0, sizeof(ent));
    memcpy(ent.task_record_id, cap_id, 20);
    memcpy(ent.derive_to, z, 20);
    memset(ent.inherit_from, 0xFF, 20);
    return tpb_raf_entry_append_task(tpb_test_task_ws, &ent);
}

/* Return 1 if `s` contains a run of >=40 lowercase hex digit characters. */
static int
has_hex40(const char *s)
{
    int run = 0;

    for (; *s != '\0'; s++) {
        if (isxdigit((unsigned char)*s) && !isupper((unsigned char)*s)) {
            run++;
            if (run >= 40) {
                return 1;
            }
        } else {
            run = 0;
        }
    }
    return 0;
}

static int
test_b7_20_name_csv(void)
{
    char **names = NULL;
    int n = 0;
    int err;

    err = tpbcli_task_parse_name_csv("a,\"b,c\",d", &names, &n);
    if (err != 0 || n != 3) {
        FAIL("B7.20: csv parse count");
        return 1;
    }
    if (strcmp(names[0], "a") != 0 || strcmp(names[1], "b,c") != 0 ||
        strcmp(names[2], "d") != 0) {
        FAIL("B7.20: csv parse values");
        return 1;
    }
    for (int i = 0; i < n; i++) {
        free(names[i]);
    }
    free(names);
    PASS();
    return 0;
}

static int
test_b7_21_resolve_prefix(void)
{
    unsigned char id[20];
    unsigned char tid[20];
    int err;

    memset(tid, 0x33, 20);
    err = write_task_metric(tid, (unsigned char[20]){0}, "Triad",
                            (double[]){1.0}, 1);
    if (err != 0) {
        FAIL("B7.21: setup task");
        return 1;
    }
    {
        char hx[41];
        char prefix[21];
        tpb_raf_id_to_hex(tid, hx);
        snprintf(prefix, sizeof(prefix), "%.12s", hx);
        err = tpbcli_task_resolve_id_prefix(g_ws, prefix, id);
    }
    if (err != 0 || memcmp(id, tid, 20) != 0) {
        FAIL("B7.21: prefix resolve");
        return 1;
    }
    PASS();
    return 0;
}

static int
test_b7_22_pooled_not_mean_of_means(void)
{
    unsigned char cap[20];
    unsigned char m0[20];
    unsigned char m1[20];
    unsigned char z[20] = {0};
    task_attr_t cattr;
    tpb_meta_header_t ch;
    tpbcli_task_init_sel_t sel;
    tpbcli_task_logical_t *tasks = NULL;
    int ntasks = 0;
    tpbcli_task_gr_opts_t opts;
    char capfile[PATH_MAX];
    FILE *fp;
    int err;

    memset(m0, 0x40, 20);
    memset(m1, 0x41, 20);
    memset(cap, 0x50, 20);
    if (write_task_metric(m0, cap, "Triad", (double[]){10.0, 20.0}, 2) != 0 ||
        write_task_metric(m1, cap, "Triad", (double[]){100.0}, 1) != 0) {
        FAIL("B7.22: member setup");
        return 1;
    }
    memset(&cattr, 0, sizeof(cattr));
    memcpy(cattr.task_record_id, cap, 20);
    memset(cattr.derive_to, 0, 20);
    memset(cattr.inherit_from, 0xFF, 20);
    cattr.ninput = 0;
    cattr.noutput = 0;
    cattr.nheader = 1;
    ch = make_out_hdr("TaskID", TPB_UNSIGNED_CHAR_T, TPB_UNIT_UNDEF, 2);
    snprintf(ch.tag, sizeof(ch.tag), "%s", TPB_TAG_LINK);
    ch.data_size = 40;
    ch.dimsizes[0] = 2;
    cattr.headers = &ch;
    {
        unsigned char blob[40];
        memcpy(blob, m0, 20);
        memcpy(blob + 20, m1, 20);
        err = tpb_raf_record_write_task(g_ws, &cattr, blob, 40);
    }
    if (err != 0) {
        FAIL("B7.22: capsule write");
        return 1;
    }
    {
        task_entry_t ent;
        memset(&ent, 0, sizeof(ent));
        memcpy(ent.task_record_id, cap, 20);
        memcpy(ent.derive_to, z, 20);
        memset(ent.inherit_from, 0xFF, 20);
        (void)tpb_raf_entry_append_task(g_ws, &ent);
    }

    snprintf(capfile, sizeof(capfile), "%s/.tmp", g_ws);
    mkdir(capfile, 0700);
    snprintf(capfile, sizeof(capfile), "%s/.tmp/%s", g_ws,
             TPBCLI_TASK_RIDMAP_NAME);
    fp = fopen(capfile, "w");
    if (fp == NULL) {
        FAIL("B7.22: ridmap");
        return 1;
    }
    {
        char hx[41];
        tpb_raf_id_to_hex(cap, hx);
        fprintf(fp, "%s\n", hx);
    }
    fclose(fp);

    memset(&sel, 0, sizeof(sel));
    memcpy(sel.id, cap, 20);
    snprintf(sel.ref, sizeof(sel.ref), "r0");
    err = tpbcli_task_build_logical_tasks(g_ws, &sel, 1, &tasks, &ntasks);
    if (err != 0 || ntasks != 1 || !tasks[0].is_capsule) {
        FAIL("B7.22: logical capsule");
        tpbcli_task_free_logical_tasks(tasks, ntasks);
        return 1;
    }
    tpbcli_task_free_logical_tasks(tasks, ntasks);

    memset(&opts, 0, sizeof(opts));
    opts.sel_kind = TPBCLI_TASK_SEL_RID;
    opts.rid_spec = "0";
    opts.ndata_names = 1;
    opts.data_names[0] = strdup("Triad");
    err = tpbcli_task_get_result(g_ws, &opts);
    free(opts.data_names[0]);
    if (err != 0) {
        FAIL("B7.22: get-result exit");
        return 1;
    }
    /* pooled mean (10+20+100)/3 = 43.3333; mean-of-means would be 57.5 */
    PASS();
    return 0;
}

static int
test_b7_23_metric_missing(void)
{
    unsigned char tid[20];
    tpbcli_task_gr_opts_t opts;
    int err;

    memset(tid, 0x77, 20);
    if (write_task_metric(tid, (unsigned char[20]){0}, "Copy",
                          (double[]){1.0}, 1) != 0) {
        FAIL("B7.23: setup");
        return 1;
    }
  {
    char capfile[PATH_MAX];
    FILE *fp;
    snprintf(capfile, sizeof(capfile), "%s/.tmp", g_ws);
    mkdir(capfile, 0700);
    snprintf(capfile, sizeof(capfile), "%s/.tmp/%s", g_ws,
             TPBCLI_TASK_RIDMAP_NAME);
    fp = fopen(capfile, "w");
    if (fp != NULL) {
        char hx[41];
        tpb_raf_id_to_hex(tid, hx);
        fprintf(fp, "%s\n", hx);
        fclose(fp);
    }
  }
    memset(&opts, 0, sizeof(opts));
    opts.sel_kind = TPBCLI_TASK_SEL_TASK_ID;
    {
        char hx[41];
        char prefix[21];
        tpb_raf_id_to_hex(tid, hx);
        snprintf(prefix, sizeof(prefix), "%.12s", hx);
        opts.task_id_spec = strdup(prefix);
    }
    opts.ndata_names = 1;
    opts.data_names[0] = strdup("MissingMetric");
    err = tpbcli_task_get_result(g_ws, &opts);
    free(opts.data_names[0]);
    if (TPBE_CAUSE(err) != TPBE_METRIC_MISSING) {
        FAIL("B7.23: expected METRIC_MISSING");
        return 1;
    }
    PASS();
    return 0;
}

static int
test_b7_24_follow_member(void)
{
    unsigned char cap[20];
    unsigned char mem[20];
    unsigned char root[20];
    int err;

    memset(cap, 0x60, 20);
    memset(mem, 0x61, 20);
    if (write_task_metric(mem, cap, "Triad", (double[]){5.0}, 1) != 0) {
        FAIL("B7.24: setup member");
        return 1;
    }
    {
        task_attr_t cattr;
        tpb_meta_header_t ch;
        unsigned char blob[20];

        memset(&cattr, 0, sizeof(cattr));
        memcpy(cattr.task_record_id, cap, 20);
        memset(cattr.derive_to, 0, 20);
        memset(cattr.inherit_from, 0xFF, 20);
        cattr.ninput = 0;
        cattr.noutput = 0;
        cattr.nheader = 1;
        ch = make_out_hdr("TaskID", TPB_UNSIGNED_CHAR_T, TPB_UNIT_UNDEF, 1);
        snprintf(ch.tag, sizeof(ch.tag), "%s", TPB_TAG_LINK);
        ch.data_size = 20;
        cattr.headers = &ch;
        memcpy(blob, mem, 20);
        if (tpb_raf_record_write_task(g_ws, &cattr, blob, 20) != 0) {
            FAIL("B7.24: capsule record");
            return 1;
        }
    }
    err = tpbcli_task_follow_derive(g_ws, mem, root);
    if (err != 0 || memcmp(root, cap, 20) != 0) {
        FAIL("B7.24: follow derive");
        return 1;
    }
    PASS();
    return 0;
}

static int
_sf_seed_ridmap_one_triad(void)
{
    unsigned char tid[20];
    unsigned char z[20] = {0};

    memset(tid, 0x88, 20);
    if (tpb_test_task_write_metric_task(tid, z, "Triad", (double[]){42.0}, 1) !=
        0) {
        return -1;
    }
    return tpb_test_task_run_cmd("ls", NULL, 0);
}

static int
test_b7_25_gr_alias(void)
{
    char buf[16384];
    int code;

    tpb_test_task_fixture_setup();
    if (_sf_seed_ridmap_one_triad() != 0) {
        tpb_test_task_fixture_cleanup();
        FAIL("B7.25: seed");
        return 1;
    }
    code = tpb_test_task_run_cmd("gr -r 0 --meta-name ref", buf, sizeof(buf));
    if (code != 0 || strstr(buf, "Meta Data") == NULL) {
        FAIL("B7.25: gr alias");
        fprintf(stderr, "    output: %.400s\n", buf);
        tpb_test_task_fixture_cleanup();
        return 1;
    }
    code = tpb_test_task_run_cmd("get-result -r 0 --meta-name ref", buf,
                                 sizeof(buf));
    tpb_test_task_fixture_cleanup();
    if (code != 0 || strstr(buf, "Meta Data") == NULL) {
        FAIL("B7.25: get-result baseline");
        return 1;
    }
    PASS();
    return 0;
}

static int
test_b7_26_meta_only(void)
{
    unsigned char tid[20];
    char buf[16384];
    int code;

    tpb_test_task_fixture_setup();
    if (tpb_test_task_write_standalone(0, tid) != TPBE_SUCCESS) {
        tpb_test_task_fixture_cleanup();
        FAIL("B7.26: seed task");
        return 1;
    }
    if (tpb_test_task_run_cmd("ls", NULL, 0) != 0) {
        tpb_test_task_fixture_cleanup();
        FAIL("B7.26: ls ridmap");
        return 1;
    }
    code = tpb_test_task_run_cmd("get-result -r 0 --meta-name ref,datetime", buf,
                                 sizeof(buf));
    if (code != 0 || strstr(buf, "Meta Data") == NULL) {
        tpb_test_task_fixture_cleanup();
        FAIL("B7.26: missing meta");
        return 1;
    }
    if (strstr(buf, "Record Data") != NULL) {
        tpb_test_task_fixture_cleanup();
        FAIL("B7.26: unexpected Record Data section");
        return 1;
    }
    if (strstr(buf, "T") == NULL || strstr(buf, "Z") == NULL) {
        tpb_test_task_fixture_cleanup();
        FAIL("B7.26: expected UTC datetime with Z");
        fprintf(stderr, "    output: %.400s\n", buf);
        return 1;
    }
    /* Explicit empty --data-name '' also omits Record Data (§7.1 / §7.7). */
    code = tpb_test_task_run_cmd("get-result -r 0 --data-name '' --meta-name ref",
                                 buf, sizeof(buf));
    tpb_test_task_fixture_cleanup();
    if (code != 0 || strstr(buf, "Meta Data") == NULL) {
        FAIL("B7.26: empty data-name meta");
        return 1;
    }
    if (strstr(buf, "Record Data") != NULL) {
        FAIL("B7.26: empty data-name still printed Record Data");
        return 1;
    }
    PASS();
    return 0;
}

static int
test_b7_27_data_only(void)
{
    char buf[16384];
    int code;

    tpb_test_task_fixture_setup();
    if (_sf_seed_ridmap_one_triad() != 0) {
        tpb_test_task_fixture_cleanup();
        FAIL("B7.27: seed");
        return 1;
    }
    code = tpb_test_task_run_cmd("get-result -r 0 --data-name Triad", buf,
                                 sizeof(buf));
    tpb_test_task_fixture_cleanup();
    if (code != 0 || strstr(buf, "Meta Data") == NULL ||
        strstr(buf, "Record Data") == NULL || strstr(buf, "Triad") == NULL) {
        FAIL("B7.27: data-only mode");
        fprintf(stderr, "    output: %.500s\n", buf);
        return 1;
    }
    PASS();
    return 0;
}

static int
test_b7_28_data_name_help(void)
{
    char buf[16384];
    int code;

    tpb_test_task_fixture_setup();
    if (_sf_seed_ridmap_one_triad() != 0) {
        tpb_test_task_fixture_cleanup();
        FAIL("B7.28: seed");
        return 1;
    }
    code = tpb_test_task_run_cmd("get-result -r 0 --data-name --help", buf,
                                 sizeof(buf));
    tpb_test_task_fixture_cleanup();
    if (code != 0 || strstr(buf, "Shared data name:") == NULL ||
        strstr(buf, "Triad") == NULL) {
        FAIL("B7.28: context data-name help");
        fprintf(stderr, "    output: %.500s\n", buf);
        return 1;
    }
    PASS();
    return 0;
}

static int
test_b7_29_show_each_subrank(void)
{
    unsigned char cap[20];
    char buf[16384];
    char expect_utc[64];
    task_attr_t cap_attr;
    void *cap_data = NULL;
    uint64_t cap_ds = 0;
    tpb_datetime_str_t utc_str;
    int code;

    tpb_test_task_fixture_setup();
    if (tpb_test_task_seed_capsule(2, cap) != 0) {
        tpb_test_task_fixture_cleanup();
        FAIL("B7.29: capsule seed");
        return 1;
    }
    if (tpb_test_task_run_cmd("ls", NULL, 0) != 0) {
        tpb_test_task_fixture_cleanup();
        FAIL("B7.29: ls ridmap");
        return 1;
    }
    code = tpb_test_task_run_cmd(
        "get-result -r 0 --meta-name ref --show-each-subrank", buf, sizeof(buf));
    if (code != 0 || strstr(buf, "Subrank") == NULL) {
        tpb_test_task_fixture_cleanup();
        FAIL("B7.29: missing Subrank column");
        fprintf(stderr, "    output: %.500s\n", buf);
        return 1;
    }
    /*
     * Capsule members are written with sleep_sep between them, so member
     * utc_bits differ. Aggregate Datetime must use the capsule entry, not
     * member consensus ("mixed").
     */
    memset(&cap_attr, 0, sizeof(cap_attr));
    if (tpb_raf_record_read_task(tpb_test_task_ws, cap, &cap_attr, &cap_data,
                                 &cap_ds) != TPBE_SUCCESS ||
        tpb_ts_bits_to_isoutc(cap_attr.utc_bits, &utc_str) != 0) {
        tpb_raf_free_headers(cap_attr.headers, cap_attr.nheader);
        free(cap_data);
        tpb_test_task_fixture_cleanup();
        FAIL("B7.29: read capsule utc");
        return 1;
    }
    snprintf(expect_utc, sizeof(expect_utc), "%s", utc_str.str);
    tpb_raf_free_headers(cap_attr.headers, cap_attr.nheader);
    free(cap_data);
    code = tpb_test_task_run_cmd("get-result -r 0 --meta-name datetime", buf,
                                 sizeof(buf));
    tpb_test_task_fixture_cleanup();
    if (code != 0 || strstr(buf, "mixed") != NULL) {
        FAIL("B7.29: capsule datetime used member consensus");
        fprintf(stderr, "    output: %.500s\n", buf);
        return 1;
    }
    if (strstr(buf, expect_utc) == NULL) {
        FAIL("B7.29: capsule datetime != work-root utc");
        fprintf(stderr, "    expect: %s\n    output: %.500s\n", expect_utc, buf);
        return 1;
    }
    PASS();
    return 0;
}

static int
test_b7_30_duplicate_header(void)
{
    task_attr_t attr;
    tpb_meta_header_t hdrs[2];
    double data[2] = {1.0, 2.0};
    unsigned char tid[20];
    char buf[16384];
    int code;

    memset(tid, 0x99, 20);
    tpb_test_task_fixture_setup();
    memset(&attr, 0, sizeof(attr));
    memcpy(attr.task_record_id, tid, 20);
    memset(attr.derive_to, 0, 20);
    memset(attr.inherit_from, 0xFF, 20);
    if (tpb_raf_hex_to_id(
            "a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2", attr.tbatch_id) != 0) {
        tpb_test_task_fixture_cleanup();
        FAIL("B7.30: ids");
        return 1;
    }
    if (tpb_raf_hex_to_id(
            "f1e2d3c4b5a6f1e2d3c4b5a6f1e2d3c4b5a6f1e2", attr.kernel_id) != 0) {
        tpb_test_task_fixture_cleanup();
        FAIL("B7.30: ids");
        return 1;
    }
    attr.ninput = 0;
    attr.noutput = 2;
    attr.nheader = 2;
    hdrs[0] = make_out_hdr("Triad", TPB_DOUBLE_T, TPB_UNIT_UNDEF, 1);
    hdrs[1] = make_out_hdr("Triad", TPB_DOUBLE_T, TPB_UNIT_UNDEF, 1);
    attr.headers = hdrs;
    if (tpb_raf_record_write_task(tpb_test_task_ws, &attr, data, 16) != 0) {
        tpb_test_task_fixture_cleanup();
        FAIL("B7.30: write");
        return 1;
    }
    {
        task_entry_t ent;
        memset(&ent, 0, sizeof(ent));
        memcpy(ent.task_record_id, tid, 20);
        (void)tpb_raf_entry_append_task(tpb_test_task_ws, &ent);
    }
    if (tpb_test_task_run_cmd("ls", NULL, 0) != 0) {
        tpb_test_task_fixture_cleanup();
        FAIL("B7.30: ls");
        return 1;
    }
    code = tpb_test_task_run_cmd("get-result -r 0 --data-name Triad", buf,
                                 sizeof(buf));
    tpb_test_task_fixture_cleanup();
    /*
     * The only requested data name is ambiguous (two same-named output
     * headers within the single member), so it cannot produce a stats
     * row. Per design 7.8, when *all* requested data is unresolvable the
     * command must fail with TPBE_METRIC_MISSING rather than report a
     * false success (a bare per-member warning must never be treated as
     * success on its own).
     */
    if (code != TPBE_METRIC_MISSING) {
        FAIL("B7.30: exit");
        fprintf(stderr, "    code: %d\n", code);
        return 1;
    }
    if (strstr(buf, "WARNING") == NULL ||
        strstr(buf, "duplicate") == NULL) {
        FAIL("B7.30: expected duplicate header warning");
        fprintf(stderr, "    output: %.600s\n", buf);
        return 1;
    }
    PASS();
    return 0;
}

/*
 * B7.31 — a capsule whose every member has a duplicate "Triad" output
 * header. No member can resolve the name unambiguously, so the command
 * must fail with TPBE_METRIC_MISSING and warn about the duplicate.
 */
static int
test_b7_31_all_members_fail_warn(void)
{
    unsigned char cap[20];
    unsigned char m0[20];
    unsigned char m1[20];
    unsigned char members[2][20];
    tpb_meta_header_t hdrs[2];
    double data[2] = {1.0, 2.0};
    char buf[16384];
    int code;

    memset(cap, 0xA0, 20);
    memset(m0, 0xA1, 20);
    memset(m1, 0xA2, 20);
    tpb_test_task_fixture_setup();

    hdrs[0] = make_out_hdr("Triad", TPB_DOUBLE_T, TPB_UNIT_UNDEF, 1);
    hdrs[1] = make_out_hdr("Triad", TPB_DOUBLE_T, TPB_UNIT_UNDEF, 1);
    if (write_member_hdrs(m0, cap, hdrs, 2, 2, data, 16) != 0 ||
        write_member_hdrs(m1, cap, hdrs, 2, 2, data, 16) != 0) {
        tpb_test_task_fixture_cleanup();
        FAIL("B7.31: member setup");
        return 1;
    }
    memcpy(members[0], m0, 20);
    memcpy(members[1], m1, 20);
    if (write_capsule_link(cap, members, 2) != 0) {
        tpb_test_task_fixture_cleanup();
        FAIL("B7.31: capsule link");
        return 1;
    }
    if (tpb_test_task_run_cmd("ls", NULL, 0) != 0) {
        tpb_test_task_fixture_cleanup();
        FAIL("B7.31: ls");
        return 1;
    }
    code = tpb_test_task_run_cmd("get-result -r 0 --data-name Triad", buf,
                                 sizeof(buf));
    tpb_test_task_fixture_cleanup();
    if (code != TPBE_METRIC_MISSING) {
        FAIL("B7.31: expected METRIC_MISSING");
        fprintf(stderr, "    code: %d\n", code);
        return 1;
    }
    if (strstr(buf, "WARNING") == NULL || strstr(buf, "duplicate") == NULL) {
        FAIL("B7.31: expected duplicate header warning");
        fprintf(stderr, "    output: %.600s\n", buf);
        return 1;
    }
    if (strstr(buf, "N/A") == NULL) {
        FAIL("B7.31: expected N/A row");
        fprintf(stderr, "    output: %.600s\n", buf);
        return 1;
    }
    PASS();
    return 0;
}

/*
 * B7.32 — a capsule with two members, both "Triad" double outputs, but the
 * second member's unit differs (TPB_UNIT_UNDEF vs TPB_UNIT_B) while shape
 * stays consistent (point). The first member establishes the schema and
 * succeeds; the second is skipped for the unit mismatch, so the command
 * reports success from 1/2 members with both warnings present.
 */
static int
test_b7_32_unit_mismatch(void)
{
    unsigned char cap[20];
    unsigned char m0[20];
    unsigned char m1[20];
    unsigned char members[2][20];
    tpb_meta_header_t h0;
    tpb_meta_header_t h1;
    double v0[1] = {10.0};
    double v1[1] = {20.0};
    char buf[16384];
    int code;

    memset(cap, 0xB0, 20);
    memset(m0, 0xB1, 20);
    memset(m1, 0xB2, 20);
    tpb_test_task_fixture_setup();

    h0 = make_out_hdr("Triad", TPB_DOUBLE_T,
                      TPB_UNIT_UNDEF | TPB_UATTR_SHAPE_POINT, 1);
    h1 = make_out_hdr("Triad", TPB_DOUBLE_T,
                      TPB_UNIT_B | TPB_UATTR_SHAPE_POINT, 1);
    if (write_member_hdrs(m0, cap, &h0, 1, 1, v0, 8) != 0 ||
        write_member_hdrs(m1, cap, &h1, 1, 1, v1, 8) != 0) {
        tpb_test_task_fixture_cleanup();
        FAIL("B7.32: member setup");
        return 1;
    }
    memcpy(members[0], m0, 20);
    memcpy(members[1], m1, 20);
    if (write_capsule_link(cap, members, 2) != 0) {
        tpb_test_task_fixture_cleanup();
        FAIL("B7.32: capsule link");
        return 1;
    }
    if (tpb_test_task_run_cmd("ls", NULL, 0) != 0) {
        tpb_test_task_fixture_cleanup();
        FAIL("B7.32: ls");
        return 1;
    }
    code = tpb_test_task_run_cmd("get-result -r 0 --data-name Triad", buf,
                                 sizeof(buf));
    tpb_test_task_fixture_cleanup();
    if (code != 0) {
        FAIL("B7.32: expected success");
        fprintf(stderr, "    code: %d\n", code);
        return 1;
    }
    if (strstr(buf, "used 1/2") == NULL &&
        strstr(buf, "unit/shape mismatch") == NULL) {
        FAIL("B7.32: expected used 1/2 or unit/shape mismatch warning");
        fprintf(stderr, "    output: %.600s\n", buf);
        return 1;
    }
    PASS();
    return 0;
}

/*
 * B7.33 — same as B7.32 but the unit matches and the shape differs
 * (TPB_UATTR_SHAPE_POINT vs TPB_UATTR_SHAPE_1D with nelem 1 vs 2). The
 * schema mismatch on shape must be treated the same way as a unit
 * mismatch: second member skipped, first member's result used.
 */
static int
test_b7_33_shape_mismatch(void)
{
    unsigned char cap[20];
    unsigned char m0[20];
    unsigned char m1[20];
    unsigned char members[2][20];
    tpb_meta_header_t h0;
    tpb_meta_header_t h1;
    double v0[1] = {10.0};
    double v1[2] = {20.0, 30.0};
    char buf[16384];
    int code;

    memset(cap, 0xC0, 20);
    memset(m0, 0xC1, 20);
    memset(m1, 0xC2, 20);
    tpb_test_task_fixture_setup();

    h0 = make_out_hdr("Triad", TPB_DOUBLE_T,
                      TPB_UNIT_UNDEF | TPB_UATTR_SHAPE_POINT, 1);
    h1 = make_out_hdr("Triad", TPB_DOUBLE_T,
                      TPB_UNIT_UNDEF | TPB_UATTR_SHAPE_1D, 2);
    if (write_member_hdrs(m0, cap, &h0, 1, 1, v0, 8) != 0 ||
        write_member_hdrs(m1, cap, &h1, 1, 1, v1, 16) != 0) {
        tpb_test_task_fixture_cleanup();
        FAIL("B7.33: member setup");
        return 1;
    }
    memcpy(members[0], m0, 20);
    memcpy(members[1], m1, 20);
    if (write_capsule_link(cap, members, 2) != 0) {
        tpb_test_task_fixture_cleanup();
        FAIL("B7.33: capsule link");
        return 1;
    }
    if (tpb_test_task_run_cmd("ls", NULL, 0) != 0) {
        tpb_test_task_fixture_cleanup();
        FAIL("B7.33: ls");
        return 1;
    }
    code = tpb_test_task_run_cmd("get-result -r 0 --data-name Triad", buf,
                                 sizeof(buf));
    tpb_test_task_fixture_cleanup();
    if (code != 0) {
        FAIL("B7.33: expected success");
        fprintf(stderr, "    code: %d\n", code);
        return 1;
    }
    if (strstr(buf, "used 1/2") == NULL &&
        strstr(buf, "unit/shape mismatch") == NULL) {
        FAIL("B7.33: expected used 1/2 or unit/shape mismatch warning");
        fprintf(stderr, "    output: %.600s\n", buf);
        return 1;
    }
    PASS();
    return 0;
}

/*
 * B7.34 — a capsule with 3 members; only member 0 has the "Triad" output,
 * the others expose an unrelated "Other" metric. Aggregation must still
 * succeed from the single usable member and warn "used 1/3 members".
 */
static int
test_b7_34_partial_members_used(void)
{
    unsigned char cap[20];
    unsigned char m0[20];
    unsigned char m1[20];
    unsigned char m2[20];
    unsigned char members[3][20];
    tpb_meta_header_t h0;
    tpb_meta_header_t hother;
    double v0[1] = {10.0};
    double vother[1] = {99.0};
    char buf[16384];
    int code;

    memset(cap, 0xD0, 20);
    memset(m0, 0xD1, 20);
    memset(m1, 0xD2, 20);
    memset(m2, 0xD3, 20);
    tpb_test_task_fixture_setup();

    h0 = make_out_hdr("Triad", TPB_DOUBLE_T, TPB_UNIT_UNDEF, 1);
    hother = make_out_hdr("Other", TPB_DOUBLE_T, TPB_UNIT_UNDEF, 1);
    if (write_member_hdrs(m0, cap, &h0, 1, 1, v0, 8) != 0 ||
        write_member_hdrs(m1, cap, &hother, 1, 1, vother, 8) != 0 ||
        write_member_hdrs(m2, cap, &hother, 1, 1, vother, 8) != 0) {
        tpb_test_task_fixture_cleanup();
        FAIL("B7.34: member setup");
        return 1;
    }
    memcpy(members[0], m0, 20);
    memcpy(members[1], m1, 20);
    memcpy(members[2], m2, 20);
    if (write_capsule_link(cap, members, 3) != 0) {
        tpb_test_task_fixture_cleanup();
        FAIL("B7.34: capsule link");
        return 1;
    }
    if (tpb_test_task_run_cmd("ls", NULL, 0) != 0) {
        tpb_test_task_fixture_cleanup();
        FAIL("B7.34: ls");
        return 1;
    }
    code = tpb_test_task_run_cmd("get-result -r 0 --data-name Triad", buf,
                                 sizeof(buf));
    tpb_test_task_fixture_cleanup();
    if (code != 0) {
        FAIL("B7.34: expected success");
        fprintf(stderr, "    code: %d\n", code);
        return 1;
    }
    if (strstr(buf, "used 1/3 members") == NULL) {
        FAIL("B7.34: expected 'used 1/3 members' warning");
        fprintf(stderr, "    output: %.600s\n", buf);
        return 1;
    }
    PASS();
    return 0;
}

/*
 * B7.35 — a standalone task written via the fixture (its tbatch_id points
 * at a tbatch that was never recorded). `--meta-name batch_host` must show
 * `N/A` plus a tbatch warning, not a bare `mixed`, and still exit 0.
 */
static int
test_b7_35_batch_host_na_warn(void)
{
    unsigned char tid[20];
    char buf[16384];
    int code;

    tpb_test_task_fixture_setup();
    if (tpb_test_task_write_standalone(0, tid) != TPBE_SUCCESS) {
        tpb_test_task_fixture_cleanup();
        FAIL("B7.35: seed task");
        return 1;
    }
    if (tpb_test_task_run_cmd("ls", NULL, 0) != 0) {
        tpb_test_task_fixture_cleanup();
        FAIL("B7.35: ls");
        return 1;
    }
    code = tpb_test_task_run_cmd("get-result -r 0 --meta-name batch_host", buf,
                                 sizeof(buf));
    tpb_test_task_fixture_cleanup();
    if (code != 0) {
        FAIL("B7.35: expected success");
        fprintf(stderr, "    code: %d\n", code);
        return 1;
    }
    if (strstr(buf, "N/A") == NULL) {
        FAIL("B7.35: expected N/A BatchHost");
        fprintf(stderr, "    output: %.600s\n", buf);
        return 1;
    }
    if (strstr(buf, "WARNING") == NULL || strstr(buf, "tbatch") == NULL) {
        FAIL("B7.35: expected tbatch warning");
        fprintf(stderr, "    output: %.600s\n", buf);
        return 1;
    }
    PASS();
    return 0;
}

/* B7.36 — request all task_attr_t scalar meta keys for one task. */
static int
test_b7_36_meta_scalar_keys(void)
{
    unsigned char tid[20];
    char buf[16384];
    int code;

    tpb_test_task_fixture_setup();
    if (tpb_test_task_write_standalone(0, tid) != TPBE_SUCCESS) {
        tpb_test_task_fixture_cleanup();
        FAIL("B7.36: seed task");
        return 1;
    }
    if (tpb_test_task_run_cmd("ls", NULL, 0) != 0) {
        tpb_test_task_fixture_cleanup();
        FAIL("B7.36: ls");
        return 1;
    }
    code = tpb_test_task_run_cmd(
        "get-result -r 0 --meta-name "
        "'task_record_id,derive_to,inherit_from,tbatch_id,kernel_id,"
        "datetime,utc_bits,btime,duration,duration_seconds,exit_code,"
        "handle_index,pid,tid,ninput,noutput,nheader,reserve'",
        buf, sizeof(buf));
    tpb_test_task_fixture_cleanup();
    if (code != 0 || strstr(buf, "Meta Data") == NULL) {
        FAIL("B7.36: expected success with Meta Data");
        fprintf(stderr, "    output: %.600s\n", buf);
        return 1;
    }
    if (strstr(buf, "T") == NULL || strstr(buf, "Z") == NULL) {
        FAIL("B7.36: expected UTC datetime with Z");
        fprintf(stderr, "    output: %.600s\n", buf);
        return 1;
    }
    if (!has_hex40(buf)) {
        FAIL("B7.36: expected a 40-hex id value");
        fprintf(stderr, "    output: %.600s\n", buf);
        return 1;
    }
    PASS();
    return 0;
}

/*
 * B7.37 — `header[INDEX].FIELD` meta keys resolve one output header's
 * fields; an unknown key or malformed header syntax fails before any table
 * is printed.
 */
static int
test_b7_37_header_field_meta(void)
{
    unsigned char tid[20];
    tpb_meta_header_t hdr;
    double data[1] = {1.0};
    char buf[16384];
    int code;

    memset(tid, 0xE1, 20);
    tpb_test_task_fixture_setup();
    hdr = make_out_hdr("Triad", TPB_DOUBLE_T, TPB_UNIT_UNDEF, 1);
    if (write_member_hdrs(tid, (unsigned char[20]){0}, &hdr, 1, 1, data, 8) !=
        0) {
        tpb_test_task_fixture_cleanup();
        FAIL("B7.37: seed task");
        return 1;
    }
    if (tpb_test_task_run_cmd("ls", NULL, 0) != 0) {
        tpb_test_task_fixture_cleanup();
        FAIL("B7.37: ls");
        return 1;
    }
    code = tpb_test_task_run_cmd(
        "get-result -r 0 --meta-name 'header[0].name,header[0].uattr_bits.shape'",
        buf, sizeof(buf));
    if (code != 0 || strstr(buf, "Triad") == NULL) {
        FAIL("B7.37: expected Triad in header field output");
        fprintf(stderr, "    output: %.600s\n", buf);
        tpb_test_task_fixture_cleanup();
        return 1;
    }
    code = tpb_test_task_run_cmd("get-result -r 0 --meta-name bogus_key", buf,
                                 sizeof(buf));
    if (code == 0 || strstr(buf, "Meta Data") != NULL ||
        strstr(buf, "Selected tasks") != NULL) {
        FAIL("B7.37: expected early failure for unknown meta key");
        fprintf(stderr, "    code=%d output: %.600s\n", code, buf);
        tpb_test_task_fixture_cleanup();
        return 1;
    }
    code = tpb_test_task_run_cmd("get-result -r 0 --meta-name 'header[abc].name'",
                                 buf, sizeof(buf));
    tpb_test_task_fixture_cleanup();
    if (code == 0 || strstr(buf, "Meta Data") != NULL ||
        strstr(buf, "Selected tasks") != NULL) {
        FAIL("B7.37: expected early failure for malformed header key");
        fprintf(stderr, "    code=%d output: %.600s\n", code, buf);
        return 1;
    }
    PASS();
    return 0;
}

/*
 * B7.38 — `--meta-name --help` on two standalone tasks must print both the
 * "Shared meta name:" and "Private meta name (ref=...):" lines.
 */
static int
test_b7_38_name_report_meta(void)
{
    unsigned char t0[20];
    unsigned char t1[20];
    char buf[16384];
    int code;

    tpb_test_task_fixture_setup();
    if (tpb_test_task_write_standalone(0, t0) != TPBE_SUCCESS) {
        tpb_test_task_fixture_cleanup();
        FAIL("B7.38: seed task 0");
        return 1;
    }
    tpb_test_task_sleep_sep();
    if (tpb_test_task_write_standalone(0, t1) != TPBE_SUCCESS) {
        tpb_test_task_fixture_cleanup();
        FAIL("B7.38: seed task 1");
        return 1;
    }
    if (tpb_test_task_run_cmd("ls", NULL, 0) != 0) {
        tpb_test_task_fixture_cleanup();
        FAIL("B7.38: ls");
        return 1;
    }
    code = tpb_test_task_run_cmd("get-result -r 0,1 --meta-name --help", buf,
                                 sizeof(buf));
    tpb_test_task_fixture_cleanup();
    if (code != 0 || strstr(buf, "Shared meta name:") == NULL ||
        strstr(buf, "Private meta name (ref=") == NULL) {
        FAIL("B7.38: expected Shared/Private meta name lines");
        fprintf(stderr, "    output: %.700s\n", buf);
        return 1;
    }
    PASS();
    return 0;
}

static int
_sf_unit_filter(const char *filter)
{
    if (filter == NULL) {
        return 1;
    }
    return (strcmp(filter, "B7.20") == 0 || strcmp(filter, "B7.21") == 0 ||
            strcmp(filter, "B7.22") == 0 || strcmp(filter, "B7.23") == 0 ||
            strcmp(filter, "B7.24") == 0);
}

int
main(int argc, char **argv)
{
    const char *filter = (argc > 1) ? argv[1] : NULL;
    int fail = 0;

    if (_sf_unit_filter(filter)) {
        setup_ws();
    }

    if (filter == NULL || strcmp(filter, "B7.20") == 0) {
        fail += test_b7_20_name_csv();
    }
    if (filter == NULL || strcmp(filter, "B7.21") == 0) {
        fail += test_b7_21_resolve_prefix();
    }
    if (filter == NULL || strcmp(filter, "B7.22") == 0) {
        fail += test_b7_22_pooled_not_mean_of_means();
    }
    if (filter == NULL || strcmp(filter, "B7.23") == 0) {
        fail += test_b7_23_metric_missing();
    }
    if (filter == NULL || strcmp(filter, "B7.24") == 0) {
        fail += test_b7_24_follow_member();
    }

    if (_sf_unit_filter(filter)) {
        cleanup_ws();
    }

    if (filter == NULL || strcmp(filter, "B7.25") == 0) {
        fail += test_b7_25_gr_alias();
    }
    if (filter == NULL || strcmp(filter, "B7.26") == 0) {
        fail += test_b7_26_meta_only();
    }
    if (filter == NULL || strcmp(filter, "B7.27") == 0) {
        fail += test_b7_27_data_only();
    }
    if (filter == NULL || strcmp(filter, "B7.28") == 0) {
        fail += test_b7_28_data_name_help();
    }
    if (filter == NULL || strcmp(filter, "B7.29") == 0) {
        fail += test_b7_29_show_each_subrank();
    }
    if (filter == NULL || strcmp(filter, "B7.30") == 0) {
        fail += test_b7_30_duplicate_header();
    }
    if (filter == NULL || strcmp(filter, "B7.31") == 0) {
        fail += test_b7_31_all_members_fail_warn();
    }
    if (filter == NULL || strcmp(filter, "B7.32") == 0) {
        fail += test_b7_32_unit_mismatch();
    }
    if (filter == NULL || strcmp(filter, "B7.33") == 0) {
        fail += test_b7_33_shape_mismatch();
    }
    if (filter == NULL || strcmp(filter, "B7.34") == 0) {
        fail += test_b7_34_partial_members_used();
    }
    if (filter == NULL || strcmp(filter, "B7.35") == 0) {
        fail += test_b7_35_batch_host_na_warn();
    }
    if (filter == NULL || strcmp(filter, "B7.36") == 0) {
        fail += test_b7_36_meta_scalar_keys();
    }
    if (filter == NULL || strcmp(filter, "B7.37") == 0) {
        fail += test_b7_37_header_field_meta();
    }
    if (filter == NULL || strcmp(filter, "B7.38") == 0) {
        fail += test_b7_38_name_report_meta();
    }

    printf("B7 get-result: %d passed, %d failed\n", g_pass, g_fail);
    return fail > 0 ? 1 : 0;
}
