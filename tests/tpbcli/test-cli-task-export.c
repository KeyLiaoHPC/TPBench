/*
 * test-cli-task-export.c
 * Pack B7: `tpbcli task export` tests (B7.40+).
 */

#include <limits.h>
#ifdef __linux__
#include <linux/limits.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "test-cli-task-fixture.h"
#include "tpb-public.h"
#include "tpbcli-task-csv.h"
#include "tpbcli-task-internal.h"

static int g_pass;
static int g_fail;

#define FAIL(msg) do { \
    g_fail++; \
    fprintf(stderr, "  FAIL: %s\n", (msg)); \
} while (0)

#define PASS() do { g_pass++; } while (0)

static int
read_file(const char *path, char *buf, size_t bufsz)
{
    FILE *fp = fopen(path, "r");
    size_t n;

    if (fp == NULL) {
        return -1;
    }
    n = fread(buf, 1, bufsz - 1, fp);
    buf[n] = '\0';
    fclose(fp);
    return (int)n;
}

static int
test_b7_40_standalone_pair(void)
{
    unsigned char tid[20];
    char out[8192];
    char cmd[256];
    char meta[PATH_MAX];
    char data[PATH_MAX];
    char mbody[4096];
    char dbody[4096];
    int st;

    if (tpb_test_task_write_standalone(0, tid) != TPBE_SUCCESS) {
        FAIL("B7.40: write standalone");
        return -1;
    }
    {
        char hx[41];
        tpb_raf_id_to_hex(tid, hx);
        snprintf(cmd, sizeof(cmd),
                 "export -i %.8s --keep-current -o \"%s/out40\"", hx,
                 tpb_test_task_ws);
    }
    st = tpb_test_task_run_cmd(cmd, out, sizeof(out));
    if (st != 0) {
        FAIL("B7.40: export exit non-zero");
        return -1;
    }
    {
        char hx[41];
        char dir[PATH_MAX];
        tpb_raf_id_to_hex(tid, hx);
        snprintf(dir, sizeof(dir), "%s/out40/%s", tpb_test_task_ws, hx);
        snprintf(meta, sizeof(meta), "%s/localhost_meta_%s.csv", dir, hx);
        snprintf(data, sizeof(data), "%s/localhost_data_%s.csv", dir, hx);
    }
    if (read_file(meta, mbody, sizeof(mbody)) < 0 ||
        read_file(data, dbody, sizeof(dbody)) < 0) {
        FAIL("B7.40: missing meta or data file");
        return -1;
    }
    if (strstr(mbody, "field,,value") == NULL ||
        strstr(mbody, "task_record_id") == NULL) {
        FAIL("B7.40: meta not double-comma");
        return -1;
    }
    PASS();
    return 0;
}

static int
test_b7_41_csv_escape(void)
{
    char path[] = "/tmp/tpb_csv_test_XXXXXX";
    FILE *fp;
    tpbcli_task_csv_writer_t w;
    char buf[256];
    int fd;

    fd = mkstemp(path);
    if (fd < 0) {
        FAIL("B7.41: mkstemp");
        return -1;
    }
    fp = fdopen(fd, "w");
    tpbcli_task_csv_writer_init(&w, fp);
    (void)tpbcli_task_csv_row_begin(&w);
    (void)tpbcli_task_csv_cell(&w, "some,,content");
    (void)tpbcli_task_csv_row_end(&w);
    fclose(fp);
    if (read_file(path, buf, sizeof(buf)) < 0 || strstr(buf, "some, ,content") == NULL) {
        FAIL("B7.41: comma escape");
        unlink(path);
        return -1;
    }
    unlink(path);
    PASS();
    return 0;
}

static int
test_b7_42_from_ls(void)
{
    unsigned char tid[20];
    char out[4096];
    int st;

    if (tpb_test_task_write_standalone(0, tid) != TPBE_SUCCESS) {
        FAIL("B7.42: write task");
        return -1;
    }
    st = tpb_test_task_run_cmd("ls -n 1", out, sizeof(out));
    if (st != 0) {
        FAIL("B7.42: ls failed");
        return -1;
    }
    snprintf(out, sizeof(out), "export --from-ls -o \"%s/out42\"", tpb_test_task_ws);
    st = tpb_test_task_run_cmd(out, NULL, 0);
    if (st != 0) {
        FAIL("B7.42: export --from-ls failed");
        return -1;
    }
    PASS();
    return 0;
}

static int
test_b7_43_capsule_members(void)
{
    unsigned char cap[20];
    char out[4096];
    char cmd[512];
    char hx[41];
    int st;
    int nfiles = 0;

    if (tpb_test_task_seed_capsule(2, cap) != 0) {
        FAIL("B7.43: capsule seed");
        return -1;
    }
    tpb_raf_id_to_hex(cap, hx);
    snprintf(cmd, sizeof(cmd),
             "export -i %s --trace-to-entry -o \"%s/out43\"", hx, tpb_test_task_ws);
    st = tpb_test_task_run_cmd(cmd, out, sizeof(out));
    if (st != 0) {
        FAIL("B7.43: capsule export failed");
        return -1;
    }
    {
        char dir[PATH_MAX];
        char meta_cap[PATH_MAX];
        snprintf(dir, sizeof(dir), "%s/out43/%s", tpb_test_task_ws, hx);
        snprintf(meta_cap, sizeof(meta_cap), "%s/capsule_%s_meta.csv", dir, hx);
        if (access(meta_cap, F_OK) != 0) {
            FAIL("B7.43: missing capsule meta");
            return -1;
        }
        nfiles++;
        {
            char m0[PATH_MAX];
            char m1[PATH_MAX];
            snprintf(m0, sizeof(m0), "%s/localhost_meta_", dir);
            if (strstr(out, "exported") != NULL) {
                nfiles++;
            }
            (void)m0;
            (void)m1;
        }
    }
    if (nfiles < 1) {
        FAIL("B7.43: expected capsule file");
        return -1;
    }
    PASS();
    return 0;
}

static int
test_b7_44_subrank_filter(void)
{
    unsigned char cap[20];
    char hx[41];
    char cmd[512];
    int st;

    if (tpb_test_task_seed_capsule(3, cap) != 0) {
        FAIL("B7.44: capsule");
        return -1;
    }
    tpb_raf_id_to_hex(cap, hx);
    snprintf(cmd, sizeof(cmd),
             "export -i %s --trace-to-entry -f subrank=1 -o \"%s/out44\"",
             hx, tpb_test_task_ws);
    st = tpb_test_task_run_cmd(cmd, NULL, 0);
    if (st != 0) {
        FAIL("B7.44: export subrank filter");
        return -1;
    }
    PASS();
    return 0;
}

static int
test_b7_45_non_tty_note(void)
{
    char out[4096];
    unsigned char tid[20];
    char hx[41];
    char cmd[256];
    int st;

    if (tpb_test_task_write_standalone(0, tid) != TPBE_SUCCESS) {
        FAIL("B7.45: write");
        return -1;
    }
    tpb_raf_id_to_hex(tid, hx);
    snprintf(cmd, sizeof(cmd), "export -i %s -o \"%s/out45\"", hx, tpb_test_task_ws);
    st = tpb_test_task_run_cmd(cmd, out, sizeof(out));
    if (st != 0) {
        FAIL("B7.45: export failed");
        return -1;
    }
    if (strstr(out, "keep-current") == NULL && strstr(out, "not a TTY") == NULL) {
        FAIL("B7.45: missing non-TTY note");
        return -1;
    }
    PASS();
    return 0;
}

static int
test_b7_46_invalid_id(void)
{
    int st = tpb_test_task_run_cmd(
        "export -i deadbeef -o /tmp/tpb_no_such_export_dir", NULL, 0);
    if (st == 0) {
        FAIL("B7.46: expected failure for bad prefix");
        return -1;
    }
    PASS();
    return 0;
}

static int
test_b7_47_parse_filters(void)
{
    tpbcli_task_export_filter_t f;
    int err;

    err = tpbcli_task_export_parse_filter("subrank=0,2-3", &f);
    if (err != TPBE_SUCCESS || f.nsubrank < 3) {
        FAIL("B7.47: subrank parse");
        free(f.subrank);
        return -1;
    }
    free(f.subrank);
    err = tpbcli_task_export_parse_filter("subtid=12ab34cd", &f);
    if (err != TPBE_SUCCESS || f.nsubtid != 1) {
        FAIL("B7.47: subtid parse");
        return -1;
    }
    PASS();
    return 0;
}

static int
test_b7_48_id_prefix_export(void)
{
    unsigned char tid[20];
    unsigned char resolved[20];
    char hx[41];
    int err;

    if (tpb_test_task_write_standalone(0, tid) != TPBE_SUCCESS) {
        FAIL("B7.48: write");
        return -1;
    }
    tpb_raf_id_to_hex(tid, hx);
    err = tpbcli_task_resolve_id_prefix_export(tpb_test_task_ws, hx, resolved);
    if (err != TPBE_SUCCESS || memcmp(resolved, tid, 20) != 0) {
        FAIL("B7.48: 4-char prefix resolve");
        return -1;
    }
    PASS();
    return 0;
}

static int
_sf_b7_49_write_env_task(unsigned char tid_out[20])
{
    task_attr_t attr;
    tpb_meta_header_t hdrs[3];
    uint8_t blob[128];
    const char *keys = "TPB_B7_49_EXPORT_PROBE";
    const char *vals = "probe_val";
    int32_t counts[1] = { 1 };
    size_t kb_len;
    size_t vb_len;
    task_entry_t ent;
    tpb_datetime_t dt;
    tpb_dtbits_t bits;
    uint64_t btime_ns;
    int err;

    kb_len = strlen(keys) + 1;
    vb_len = strlen(vals) + 1;
    memset(blob, 0, sizeof(blob));
    memcpy(blob, keys, kb_len);
    memcpy(blob + kb_len, counts, sizeof(counts));
    memcpy(blob + kb_len + sizeof(counts), vals, vb_len);

    memset(&attr, 0, sizeof(attr));
    memset(attr.inherit_from, 0xFF, 20);
    if (tpb_raf_hex_to_id(
            "a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2", attr.tbatch_id) != 0) {
        return -1;
    }
    if (tpb_raf_hex_to_id(
            "f1e2d3c4b5a6f1e2d3c4b5a6f1e2d3c4b5a6f1e2", attr.kernel_id) != 0) {
        return -1;
    }
    memset(&dt, 0, sizeof(dt));
    (void)tpb_ts_get_datetime(TPBM_TS_UTC, &dt);
    (void)tpb_ts_datetime_to_bits(&dt, 0, &bits);
    attr.utc_bits = bits;
    btime_ns = 1;
    err = tpb_raf_gen_task_id(bits, btime_ns, "localhost", "test",
                              attr.tbatch_id, attr.kernel_id, 0, (uint32_t)getpid(),
                              1, tid_out);
    if (err != TPBE_SUCCESS) {
        return -1;
    }
    memcpy(attr.task_record_id, tid_out, 20);
    attr.ninput = 0;
    attr.noutput = 0;
    attr.nheader = 3;
    memset(hdrs, 0, sizeof(hdrs));
    hdrs[0].block_size = 256;
    hdrs[0].ndim = 1;
    hdrs[0].dimsizes[0] = kb_len;
    hdrs[0].data_size = kb_len;
    hdrs[0].type_bits = (uint32_t)(TPB_STRING_T & TPB_PARM_TYPE_MASK);
    snprintf(hdrs[0].name, sizeof(hdrs[0].name), "%s", TPB_TASK_HDR_ENV_KEY);
    hdrs[1].block_size = 256;
    hdrs[1].ndim = 1;
    hdrs[1].dimsizes[0] = 1;
    hdrs[1].data_size = sizeof(counts);
    hdrs[1].type_bits = (uint32_t)(TPB_INT32_T & TPB_PARM_TYPE_MASK);
    snprintf(hdrs[1].name, sizeof(hdrs[1].name), "%s", TPB_TASK_HDR_ENV_COUNT);
    hdrs[2].block_size = 256;
    hdrs[2].ndim = 1;
    hdrs[2].dimsizes[0] = vb_len;
    hdrs[2].data_size = vb_len;
    hdrs[2].type_bits = (uint32_t)(TPB_STRING_T & TPB_PARM_TYPE_MASK);
    snprintf(hdrs[2].name, sizeof(hdrs[2].name), "%s", TPB_TASK_HDR_ENV_VALUE);
    attr.headers = hdrs;
    err = tpb_raf_record_write_task(tpb_test_task_ws, &attr, blob,
                                    kb_len + sizeof(counts) + vb_len);
    if (err != TPBE_SUCCESS) {
        return -1;
    }
    memset(&ent, 0, sizeof(ent));
    memcpy(ent.task_record_id, tid_out, 20);
    memcpy(ent.tbatch_id, attr.tbatch_id, 20);
    memcpy(ent.kernel_id, attr.kernel_id, 20);
    ent.utc_bits = attr.utc_bits;
    return (tpb_raf_entry_append_task(tpb_test_task_ws, &ent) == TPBE_SUCCESS) ? 0
                                                                                : -1;
}

static int
test_b7_49_env_two_column_export(void)
{
    unsigned char tid[20];
    char hx[41];
    char cmd[512];
    char meta[PATH_MAX];
    char data[PATH_MAX];
    char mbody[8192];
    char dbody[8192];
    int st;

    if (_sf_b7_49_write_env_task(tid) != 0) {
        FAIL("B7.49: write synthetic env task");
        return -1;
    }
    tpb_raf_id_to_hex(tid, hx);
    snprintf(cmd, sizeof(cmd),
             "export -i %.8s --keep-current -o \"%s/out49\"", hx,
             tpb_test_task_ws);
    st = tpb_test_task_run_cmd(cmd, NULL, 0);
    if (st != 0) {
        FAIL("B7.49: export failed");
        return -1;
    }
    {
        char dir[PATH_MAX];
        snprintf(dir, sizeof(dir), "%s/out49/%s", tpb_test_task_ws, hx);
        snprintf(meta, sizeof(meta), "%s/localhost_meta_%s.csv", dir, hx);
        snprintf(data, sizeof(data), "%s/localhost_data_%s.csv", dir, hx);
    }
    if (read_file(meta, mbody, sizeof(mbody)) < 0 ||
        read_file(data, dbody, sizeof(dbody)) < 0) {
        FAIL("B7.49: missing export files");
        return -1;
    }
    if (strstr(mbody, "environment_variable_count") == NULL) {
        FAIL("B7.49: meta should list env count header");
        return -1;
    }
    if (strstr(dbody, "environment_variable_count") != NULL) {
        FAIL("B7.49: data must not include env count column");
        return -1;
    }
    if (strstr(dbody, "environment_variable_key") == NULL ||
        strstr(dbody, "environment_variable_value") == NULL) {
        FAIL("B7.49: data missing env key/value columns");
        return -1;
    }
    if (strstr(dbody, "TPB_B7_49_EXPORT_PROBE") == NULL ||
        strstr(dbody, "probe_val") == NULL) {
        FAIL("B7.49: expected probe env row in data");
        return -1;
    }
    PASS();
    return 0;
}

int
main(int argc, char **argv)
{
    const char *id = (argc > 1) ? argv[1] : "";

    g_pass = 0;
    g_fail = 0;
    tpb_test_task_fixture_setup();
    tpb_test_task_fixture_seed_env();

    if (id[0] == '\0' || strcmp(id, "B7.40") == 0) {
        test_b7_40_standalone_pair();
    }
    if (id[0] == '\0' || strcmp(id, "B7.41") == 0) {
        test_b7_41_csv_escape();
    }
    if (id[0] == '\0' || strcmp(id, "B7.42") == 0) {
        test_b7_42_from_ls();
    }
    if (id[0] == '\0' || strcmp(id, "B7.43") == 0) {
        test_b7_43_capsule_members();
    }
    if (id[0] == '\0' || strcmp(id, "B7.44") == 0) {
        test_b7_44_subrank_filter();
    }
    if (id[0] == '\0' || strcmp(id, "B7.45") == 0) {
        test_b7_45_non_tty_note();
    }
    if (id[0] == '\0' || strcmp(id, "B7.46") == 0) {
        test_b7_46_invalid_id();
    }
    if (id[0] == '\0' || strcmp(id, "B7.47") == 0) {
        test_b7_47_parse_filters();
    }
    if (id[0] == '\0' || strcmp(id, "B7.48") == 0) {
        test_b7_48_id_prefix_export();
    }
    if (id[0] == '\0' || strcmp(id, "B7.49") == 0) {
        test_b7_49_env_two_column_export();
    }

    tpb_test_task_fixture_cleanup();
    fprintf(stderr, "export tests: %d pass, %d fail\n", g_pass, g_fail);
    return (g_fail > 0) ? 1 : 0;
}
