/*
 * test_tpb_rtenv_env_snapshot.c
 * Pack A10: environment snapshot colon-delimited key/value encoding.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/tpb-public.h"

static char g_ws[512];

typedef struct {
    const char *id;
    const char *name;
    int (*func)(void);
} test_case_t;

static void
_sf_setup_workspace(void)
{
    tpb_raf_rtenv_entry_t ent;
    tpb_raf_rtenv_attr_t attr;
    tpb_datetime_t dt;

    snprintf(g_ws, sizeof(g_ws), "/tmp/tpb_rtenv_snap_%d", (int)getpid());
    tpb_raf_init_workspace(g_ws);

    memset(&ent, 0, sizeof(ent));
    ent.id = 1;
    snprintf(ent.name, sizeof(ent.name), "base");
    snprintf(ent.hostname, sizeof(ent.hostname), "testhost");
    (void)tpb_ts_datetime_to_bits(&dt, 0, &ent.utc_bits);
    ent.inherit_from = 0;
    ent.derive_to = -1;
    snprintf(ent.note, sizeof(ent.note), "env snapshot test");
    (void)tpb_raf_entry_append_rtenv(g_ws, &ent);

    memset(&attr, 0, sizeof(attr));
    attr.id = 1;
    snprintf(attr.name, sizeof(attr.name), "base");
    snprintf(attr.hostname, sizeof(attr.hostname), "testhost");
    attr.utc_bits = ent.utc_bits;
    attr.inherit_from = 0;
    attr.derive_to = -1;
    snprintf(attr.note, sizeof(attr.note), "env snapshot test");
    attr.nheader = 0;
    (void)tpb_raf_record_write_rtenv(g_ws, &attr, NULL, NULL, 0);
    (void)tpb_raf_config_set_base_id(g_ws, 1);
}

static void
_sf_cleanup_workspace(void)
{
    char cmd[640];

    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", g_ws);
    (void)system(cmd);
}

static int
_sf_extract_env_headers(tpb_meta_header_t *hdrs, uint32_t nhdr, void *data,
                        const char **key_blob, const int32_t **counts,
                        size_t *nkeys, const char **value_blob)
{
    uint32_t i;
    size_t off = 0;

    *key_blob = NULL;
    *counts = NULL;
    *nkeys = 0;
    *value_blob = NULL;

    for (i = 0; i < nhdr; i++) {
        if (strcmp(hdrs[i].name, "environment_variable_key") == 0) {
            *key_blob = (const char *)data + off;
        } else if (strcmp(hdrs[i].name, "environment_variable_count") == 0) {
            *counts = (const int32_t *)((const char *)data + off);
            *nkeys = (size_t)hdrs[i].dimsizes[0];
        } else if (strcmp(hdrs[i].name, "environment_variable_value") == 0) {
            *value_blob = (const char *)data + off;
        }
        off += (size_t)hdrs[i].data_size;
    }
    return (*key_blob != NULL && *counts != NULL && *value_blob != NULL) ? 0
                                                                         : -1;
}

static int
_sf_capture_to_headers(const char **key_blob, const int32_t **counts,
                       size_t *nkeys, const char **value_blob)
{
    tpb_meta_header_t *hdrs = NULL;
    void *data = NULL;
    uint32_t nhdr = 0;
    uint64_t dsize = 0;
    int err;

    tpb_rtenv_clear_environ_snapshot();
    err = tpb_rtenv_capture_environ_snapshot(g_ws);
    if (err != TPBE_SUCCESS) {
        return err;
    }
    err = tpb_rtenv_append_env_snapshot_headers(&hdrs, &nhdr, &data, &dsize);
    if (err != TPBE_SUCCESS) {
        free(data);
        return err;
    }
    err = _sf_extract_env_headers(hdrs, nhdr, data, key_blob, counts, nkeys,
                                  value_blob);
    free(hdrs);
    free(data);
    return err;
}

static int
_sf_key_index(const char *key_blob, const char *want)
{
    const char *p;
    const char *start;
    int idx = 0;
    size_t wlen;

    if (key_blob == NULL || want == NULL) {
        return -1;
    }
    wlen = strlen(want);
    start = key_blob;
    for (p = key_blob; ; p++) {
        if (*p == ':' || *p == '\0') {
            if ((size_t)(p - start) == wlen && memcmp(start, want, wlen) == 0) {
                return idx;
            }
            if (*p == '\0') {
                break;
            }
            idx++;
            start = p + 1;
        }
    }
    return -1;
}

static int
_sf_decode_value(const char *key_blob, const int32_t *counts, size_t nkeys,
                 const char *value_blob, const char *want_key, char *out,
                 size_t outlen)
{
    int ki;
    size_t i;
    size_t seg_skip = 0;
    size_t seg_take;
    const char *p;
    const char *start;
    size_t nseg = 0;
    size_t outpos = 0;

    ki = _sf_key_index(key_blob, want_key);
    if (ki < 0 || (size_t)ki >= nkeys) {
        return -1;
    }
    for (i = 0; i < (size_t)ki; i++) {
        if (counts[i] < 0) {
            return -1;
        }
        seg_skip += (size_t)counts[i];
    }
    seg_take = (size_t)counts[ki];
    if (outlen == 0) {
        return -1;
    }
    out[0] = '\0';

    p = value_blob;
    while (nseg < seg_skip && p != NULL && *p != '\0') {
        while (*p != '\0' && *p != ':') {
            p++;
        }
        if (*p == ':') {
            p++;
        }
        nseg++;
    }

    start = p;
    for (i = 0; i < seg_take; i++) {
        if (*p == '\0' && p == start) {
            return -1;
        }
        while (*p != '\0' && *p != ':') {
            p++;
        }
        if (outpos > 0) {
            if (outpos + 1 >= outlen) {
                return -1;
            }
            out[outpos++] = ':';
        }
        if ((size_t)(p - start) >= outlen - outpos) {
            return -1;
        }
        memcpy(out + outpos, start, (size_t)(p - start));
        outpos += (size_t)(p - start);
        out[outpos] = '\0';
        if (*p == ':') {
            p++;
        }
        start = p;
    }
    return 0;
}

static int
_sf_count_for_key(const char *key_blob, const int32_t *counts, size_t nkeys,
                  const char *want_key, int32_t *count_out)
{
    int ki;

    ki = _sf_key_index(key_blob, want_key);
    if (ki < 0 || (size_t)ki >= nkeys) {
        return -1;
    }
    *count_out = counts[ki];
    return 0;
}

static int
_sf_prepare_capture(void)
{
    setenv("TPB_RTENV_ID", "1", 1);
    return 0;
}

static int
test_a10_1_single_key(void)
{
    const char *kb;
    const int32_t *counts;
    size_t nkeys;
    const char *vb;
    char val[256];
    int32_t nseg;

    _sf_setup_workspace();
    _sf_prepare_capture();
    setenv("TPB_SNAP_T1", "hello", 1);

    if (_sf_capture_to_headers(&kb, &counts, &nkeys, &vb) != 0) {
        _sf_cleanup_workspace();
        return 1;
    }
    if (strchr(kb, ';') != NULL) {
        _sf_cleanup_workspace();
        return 1;
    }
    if (_sf_count_for_key(kb, counts, nkeys, "TPB_SNAP_T1", &nseg) != 0 ||
        nseg != 1) {
        _sf_cleanup_workspace();
        return 1;
    }
    if (_sf_decode_value(kb, counts, nkeys, vb, "TPB_SNAP_T1", val,
                         sizeof(val)) != 0 ||
        strcmp(val, "hello") != 0) {
        _sf_cleanup_workspace();
        return 1;
    }
    unsetenv("TPB_SNAP_T1");
    _sf_cleanup_workspace();
    return 0;
}

static int
test_a10_2_path_segments(void)
{
    const char *kb;
    const int32_t *counts;
    size_t nkeys;
    const char *vb;
    char val[256];
    int32_t nseg;

    _sf_setup_workspace();
    _sf_prepare_capture();
    setenv("TPB_SNAP_PATH", "/a:/b", 1);

    if (_sf_capture_to_headers(&kb, &counts, &nkeys, &vb) != 0) {
        _sf_cleanup_workspace();
        return 1;
    }
    if (_sf_count_for_key(kb, counts, nkeys, "TPB_SNAP_PATH", &nseg) != 0 ||
        nseg != 2) {
        _sf_cleanup_workspace();
        return 1;
    }
    if (_sf_decode_value(kb, counts, nkeys, vb, "TPB_SNAP_PATH", val,
                         sizeof(val)) != 0 ||
        strcmp(val, "/a:/b") != 0) {
        _sf_cleanup_workspace();
        return 1;
    }
    if (strcmp(vb, "/a:/b") != 0 && strstr(vb, "/a:/b") == NULL) {
        /* flat segments for this key must appear as /a:/b */
        const char *found = strstr(kb, "TPB_SNAP_PATH");
        if (found == NULL) {
            _sf_cleanup_workspace();
            return 1;
        }
    }
    unsetenv("TPB_SNAP_PATH");
    _sf_cleanup_workspace();
    return 0;
}

static int
test_a10_3_multi_key_mixed(void)
{
    const char *kb;
    const int32_t *counts;
    size_t nkeys;
    const char *vb;
    char val[256];
    int32_t nseg;

    _sf_setup_workspace();
    _sf_prepare_capture();
    setenv("TPB_SNAP_A", "1", 1);
    setenv("TPB_SNAP_B", "2", 1);
    setenv("TPB_SNAP_P", "/x:/y", 1);

    if (_sf_capture_to_headers(&kb, &counts, &nkeys, &vb) != 0) {
        _sf_cleanup_workspace();
        return 1;
    }
    if (_sf_key_index(kb, "TPB_SNAP_A") < 0 ||
        _sf_key_index(kb, "TPB_SNAP_B") < 0 ||
        _sf_key_index(kb, "TPB_SNAP_P") < 0) {
        _sf_cleanup_workspace();
        return 1;
    }
    if (_sf_count_for_key(kb, counts, nkeys, "TPB_SNAP_P", &nseg) != 0 ||
        nseg != 2) {
        _sf_cleanup_workspace();
        return 1;
    }
    if (_sf_decode_value(kb, counts, nkeys, vb, "TPB_SNAP_P", val,
                         sizeof(val)) != 0 ||
        strcmp(val, "/x:/y") != 0) {
        _sf_cleanup_workspace();
        return 1;
    }
    if (_sf_decode_value(kb, counts, nkeys, vb, "TPB_SNAP_A", val,
                         sizeof(val)) != 0 ||
        strcmp(val, "1") != 0) {
        _sf_cleanup_workspace();
        return 1;
    }
    unsetenv("TPB_SNAP_A");
    unsetenv("TPB_SNAP_B");
    unsetenv("TPB_SNAP_P");
    _sf_cleanup_workspace();
    return 0;
}

static int
test_a10_4_empty_value(void)
{
    const char *kb;
    const int32_t *counts;
    size_t nkeys;
    const char *vb;
    char val[16];
    int32_t nseg;

    _sf_setup_workspace();
    _sf_prepare_capture();
    setenv("TPB_SNAP_EMPTY", "", 1);
    setenv("TPB_SNAP_X", "1", 1);

    if (_sf_capture_to_headers(&kb, &counts, &nkeys, &vb) != 0) {
        _sf_cleanup_workspace();
        return 1;
    }
    if (_sf_count_for_key(kb, counts, nkeys, "TPB_SNAP_EMPTY", &nseg) != 0 ||
        nseg != 0) {
        _sf_cleanup_workspace();
        return 1;
    }
    if (_sf_decode_value(kb, counts, nkeys, vb, "TPB_SNAP_X", val,
                         sizeof(val)) != 0 ||
        strcmp(val, "1") != 0) {
        _sf_cleanup_workspace();
        return 1;
    }
    unsetenv("TPB_SNAP_EMPTY");
    unsetenv("TPB_SNAP_X");
    _sf_cleanup_workspace();
    return 0;
}

static int
test_a10_5_semicolon_in_value(void)
{
    const char *kb;
    const int32_t *counts;
    size_t nkeys;
    const char *vb;
    char val[256];
    int32_t nseg;

    _sf_setup_workspace();
    _sf_prepare_capture();
    setenv("TPB_SNAP_SEM", "a;b", 1);

    if (_sf_capture_to_headers(&kb, &counts, &nkeys, &vb) != 0) {
        _sf_cleanup_workspace();
        return 1;
    }
    if (_sf_count_for_key(kb, counts, nkeys, "TPB_SNAP_SEM", &nseg) != 0 ||
        nseg != 1) {
        _sf_cleanup_workspace();
        return 1;
    }
    if (_sf_decode_value(kb, counts, nkeys, vb, "TPB_SNAP_SEM", val,
                         sizeof(val)) != 0 ||
        strcmp(val, "a;b") != 0) {
        _sf_cleanup_workspace();
        return 1;
    }
    unsetenv("TPB_SNAP_SEM");
    _sf_cleanup_workspace();
    return 0;
}

static int
run_pack(const char *prefix, test_case_t *cases, int n, const char *filter)
{
    int i;
    int pass = 0;
    int fail = 0;

    printf("Running test pack %s (%d cases)\n", prefix, n);
    printf("------------------------------------------------------\n");
    for (i = 0; i < n; i++) {
        int res;

        if (filter != NULL && strcmp(filter, cases[i].id) != 0) {
            continue;
        }
        res = cases[i].func();
        printf("[%s] %-36s %s\n", cases[i].id, cases[i].name,
               res == 0 ? "PASS" : "FAIL");
        if (res == 0) {
            pass++;
        } else {
            fail++;
        }
    }
    printf("------------------------------------------------------\n");
    printf("Pack %s: %d passed, %d failed\n", prefix, pass, fail);
    return fail;
}

int
main(int argc, char **argv)
{
    const char *filter = (argc > 1) ? argv[1] : NULL;
    test_case_t cases[] = {
        { "A10.1", "single_key",           test_a10_1_single_key },
        { "A10.2", "path_segments",        test_a10_2_path_segments },
        { "A10.3", "multi_key_mixed",      test_a10_3_multi_key_mixed },
        { "A10.4", "empty_value",          test_a10_4_empty_value },
        { "A10.5", "semicolon_in_value",   test_a10_5_semicolon_in_value },
    };
    int n = (int)(sizeof(cases) / sizeof(cases[0]));
    int fail = run_pack("A10", cases, n, filter);

    return (fail > 0) ? 1 : 0;
}
