/*
 * test_raf.c
 * Test pack A4: rafdb module unit tests (A4.1-A4.19).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "include/tpb-public.h"
#include "corelib/rafdb/tpb-raf-types.h"
#include "corelib/rafdb/rafdb-l1-types.h"
#include "corelib/rafdb/tpb-sha1.h"
#include "corelib/strftime.h"

typedef struct {
    const char *id;
    const char *name;
    int (*func)(void);
} test_case_t;

static char g_test_dir[512];

/* Local Function Prototypes */
static int run_pack(const char *prefix, test_case_t *cases,
                    int n, const char *filter);
static void setup_test_dir(void);
static void cleanup_test_dir(void);
static int test_magic_construct(void);
static int test_magic_validate_ok(void);
static int test_magic_validate_bad(void);
static int test_id_tbatch(void);
static int test_id_kernel(void);
static int test_id_task(void);
static int test_id_uniqueness(void);
static int test_entry_tbatch(void);
static int test_entry_kernel(void);
static int test_entry_task(void);
static int test_entry_multi(void);
static int test_record_tbatch(void);
static int test_record_kernel(void);
static int test_record_task(void);
static int test_header_1d(void);
static int test_header_multidim(void);
static int test_header_mixed(void);
static int test_rtenv_domain_dir(void);
static int test_rtenv_id_alloc(void);
static int test_rtenv_dup_name(void);
static int test_rtenv_entry_roundtrip(void);
static int test_rtenv_record_roundtrip(void);
static int test_rtenv_decimal_path(void);
static int test_rtenv_counter_patch(void);
static int test_rtenv_derive_link(void);
static int test_rtenv_config_base_id(void);
static int test_rtenv_resolve_active(void);
static int test_rtenv_build_snapshot(void);

static int
run_pack(const char *prefix, test_case_t *cases,
         int n, const char *filter)
{
    int i, pass = 0, fail = 0, res;

    printf("Running test pack %s (%d cases)\n", prefix, n);
    printf("------------------------------------------------------\n");

    for (i = 0; i < n; i++) {
        if (filter && strcmp(filter, cases[i].id) != 0) {
            continue;
        }
        res = cases[i].func();
        printf("[%s] %-36s %s\n", cases[i].id, cases[i].name,
               res == 0 ? "PASS" : "FAIL");
        if (res == 0) pass++;
        else fail++;
    }

    printf("------------------------------------------------------\n");
    printf("Pack %s: %d passed, %d failed\n", prefix, pass, fail);
    return fail;
}

static void
setup_test_dir(void)
{
    snprintf(g_test_dir, sizeof(g_test_dir),
             "/tmp/tpb_raf_test_%d", (int)getpid());
    mkdir(g_test_dir, 0755);

    char path[600];
    snprintf(path, sizeof(path), "%s/%s",
             g_test_dir, TPB_RAF_TBATCH_DIR);
    tpb_raf_init_workspace(g_test_dir);
}

static void
cleanup_test_dir(void)
{
    char cmd[600];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", g_test_dir);
    system(cmd);
}

/* A4.1: magic_construct */
static int
test_magic_construct(void)
{
    unsigned char m[8];
    /* tbatch entry start */
    tpb_raf_build_magic(TPB_RAF_FTYPE_ENTRY,
                          TPB_RAF_DOM_TBATCH,
                          TPB_RAF_POS_START, m);
    if (m[0] != 0xE1 || m[1] != 0x54 || m[2] != 0x50 ||
        m[3] != 0x42 || m[4] != 0xE0 || m[5] != 0x53 ||
        m[6] != 0x31 || m[7] != 0xE0) {
        return 1;
    }

    /* kernel record split */
    tpb_raf_build_magic(TPB_RAF_FTYPE_RECORD,
                          TPB_RAF_DOM_KERNEL,
                          TPB_RAF_POS_SPLIT, m);
    if (m[4] != 0xD1 || m[5] != 0x44) return 1;

    /* task entry end */
    tpb_raf_build_magic(TPB_RAF_FTYPE_ENTRY,
                          TPB_RAF_DOM_TASK,
                          TPB_RAF_POS_END, m);
    if (m[4] != 0xE2 || m[5] != 0x45) return 1;

    return 0;
}

/* A4.2: magic_validate_ok */
static int
test_magic_validate_ok(void)
{
    unsigned char m[8];
    tpb_raf_build_magic(TPB_RAF_FTYPE_ENTRY,
                          TPB_RAF_DOM_TBATCH,
                          TPB_RAF_POS_START, m);
    if (!tpb_raf_validate_magic(m, TPB_RAF_FTYPE_ENTRY,
                                  TPB_RAF_DOM_TBATCH,
                                  TPB_RAF_POS_START)) {
        return 1;
    }

    tpb_raf_build_magic(TPB_RAF_FTYPE_RECORD,
                          TPB_RAF_DOM_TASK,
                          TPB_RAF_POS_END, m);
    if (!tpb_raf_validate_magic(m, TPB_RAF_FTYPE_RECORD,
                                  TPB_RAF_DOM_TASK,
                                  TPB_RAF_POS_END)) {
        return 1;
    }
    return 0;
}

/* A4.3: magic_validate_bad */
static int
test_magic_validate_bad(void)
{
    unsigned char m[8];
    tpb_raf_build_magic(TPB_RAF_FTYPE_ENTRY,
                          TPB_RAF_DOM_TBATCH,
                          TPB_RAF_POS_START, m);

    /* Wrong domain should fail */
    if (tpb_raf_validate_magic(m, TPB_RAF_FTYPE_ENTRY,
                                 TPB_RAF_DOM_KERNEL,
                                 TPB_RAF_POS_START)) {
        return 1;
    }

    /* Corrupted byte */
    m[0] = 0x00;
    if (tpb_raf_validate_magic(m, TPB_RAF_FTYPE_ENTRY,
                                 TPB_RAF_DOM_TBATCH,
                                 TPB_RAF_POS_START)) {
        return 1;
    }
    return 0;
}

/* A4.6: id_tbatch */
static int
test_id_tbatch(void)
{
    unsigned char id1[20], id2[20];

    tpb_raf_gen_tbatch_id(12345, 67890,
                            "node01", "testuser", 1000, id1);
    tpb_raf_gen_tbatch_id(12345, 67890,
                            "node01", "testuser", 1000, id2);

    if (memcmp(id1, id2, 20) != 0) return 1;
    return 0;
}

/* A4.7: id_kernel */
static int
test_id_kernel(void)
{
    unsigned char id1[20], id2[20];
    unsigned char tpbx[20];

    memset(tpbx, 0xBB, 20);

    tpb_raf_gen_kernel_id(tpbx, id1);
    tpb_raf_gen_kernel_id(tpbx, id2);

    if (memcmp(id1, id2, 20) != 0) return 1;
    if (memcmp(id1, tpbx, 20) != 0) return 1;
    return 0;
}

/* A4.8: id_task */
static int
test_id_task(void)
{
    unsigned char id1[20], id2[20], id3[20], id4[20];
    unsigned char tb[20], kn[20];

    memset(tb, 0x11, 20);
    memset(kn, 0x22, 20);

    tpb_raf_gen_task_id(111, 222, "host", "user",
                          tb, kn, 0, 333, 444, id1);
    tpb_raf_gen_task_id(111, 222, "host", "user",
                          tb, kn, 0, 333, 444, id2);
    tpb_raf_gen_task_id(111, 222, "host", "user",
                          tb, kn, 0, 334, 444, id3);
    tpb_raf_gen_task_id(111, 222, "host", "user",
                          tb, kn, 0, 333, 445, id4);

    if (memcmp(id1, id2, 20) != 0) return 1;
    if (memcmp(id1, id3, 20) == 0) return 1;
    if (memcmp(id1, id4, 20) == 0) return 1;
    return 0;
}

/* A4.9: id_uniqueness */
static int
test_id_uniqueness(void)
{
    unsigned char id_a[20], id_b[20];
    unsigned char tpbx_a[20], tpbx_b[20];

    memset(tpbx_a, 0xAA, 20);
    memset(tpbx_b, 0xBB, 20);

    tpb_raf_gen_kernel_id(tpbx_a, id_a);
    tpb_raf_gen_kernel_id(tpbx_b, id_b);

    if (memcmp(id_a, id_b, 20) == 0) return 1;

    tpb_raf_gen_tbatch_id(100, 200, "h", "u", 1, id_a);
    tpb_raf_gen_tbatch_id(100, 200, "h", "u", 2, id_b);

    if (memcmp(id_a, id_b, 20) == 0) return 1;
    return 0;
}

/* A4.10: entry_tbatch */
static int
test_entry_tbatch(void)
{
    setup_test_dir();

    tbatch_entry_t e;
    memset(&e, 0, sizeof(e));
    memset(e.tbatch_id, 0xAA, 20);
    memset(e.inherit_from, 0xCC, 20);
    e.start_utc_bits = 12345;
    e.duration = 1000000000ULL;
    snprintf(e.hostname, 64, "testhost");
    e.nkernel = 2;
    e.ntask = 3;
    e.nscore = 0;
    e.batch_type = TPB_BATCH_TYPE_RUN;

    int err = tpb_raf_entry_append_tbatch(g_test_dir, &e);
    if (err) { cleanup_test_dir(); return 1; }

    tbatch_entry_t *entries = NULL;
    int count = 0;
    err = tpb_raf_entry_list_tbatch(g_test_dir, &entries,
                                      &count);
    if (err || count != 1) {
        free(entries);
        cleanup_test_dir();
        return 1;
    }

    if (memcmp(entries[0].tbatch_id, e.tbatch_id, 20) != 0 ||
        memcmp(entries[0].inherit_from, e.inherit_from, 20) != 0 ||
        entries[0].start_utc_bits != e.start_utc_bits ||
        entries[0].duration != e.duration ||
        entries[0].nkernel != 2 || entries[0].ntask != 3 ||
        entries[0].batch_type != TPB_BATCH_TYPE_RUN) {
        free(entries);
        cleanup_test_dir();
        return 1;
    }

    free(entries);
    cleanup_test_dir();
    return 0;
}

/* A4.11: entry_kernel */
static int
test_entry_kernel(void)
{
    setup_test_dir();

    kernel_entry_t e;
    memset(&e, 0, sizeof(e));
    memset(e.kernel_id, 0xBB, 20);
    memset(e.inherit_from, 0xDD, 20);
    snprintf(e.kernel_name, 64, "triad");
    e.kctrl = TPB_KTYPE_PLI;
    e.narg = 3;
    e.nmetric = 1;
    e.utc_bits = 12345;

    int err = tpb_raf_entry_append_kernel(g_test_dir, &e);
    if (err) { cleanup_test_dir(); return 1; }

    kernel_entry_t *entries = NULL;
    int count = 0;
    err = tpb_raf_entry_list_kernel(g_test_dir, &entries,
                                      &count);
    if (err || count != 1) {
        free(entries);
        cleanup_test_dir();
        return 1;
    }

    if (memcmp(entries[0].kernel_id, e.kernel_id, 20) != 0 ||
        memcmp(entries[0].inherit_from, e.inherit_from, 20) != 0 ||
        strcmp(entries[0].kernel_name, "triad") != 0 ||
        entries[0].narg != 3 || entries[0].nmetric != 1 ||
        entries[0].utc_bits != e.utc_bits) {
        free(entries);
        cleanup_test_dir();
        return 1;
    }

    free(entries);
    cleanup_test_dir();
    return 0;
}

/* A4.12: entry_task */
static int
test_entry_task(void)
{
    setup_test_dir();

    task_entry_t e;
    memset(&e, 0, sizeof(e));
    memset(e.task_record_id, 0xDD, 20);
    memset(e.inherit_from, 0xEE, 20);
    memset(e.tbatch_id, 0xAA, 20);
    memset(e.kernel_id, 0xBB, 20);
    e.utc_bits = 99999;
    e.duration = 500000000ULL;
    e.exit_code = 0;
    e.handle_index = 1;

    int err = tpb_raf_entry_append_task(g_test_dir, &e);
    if (err) { cleanup_test_dir(); return 1; }

    task_entry_t *entries = NULL;
    int count = 0;
    err = tpb_raf_entry_list_task(g_test_dir, &entries,
                                    &count);
    if (err || count != 1) {
        free(entries);
        cleanup_test_dir();
        return 1;
    }

    if (memcmp(entries[0].task_record_id,
               e.task_record_id, 20) != 0 ||
        memcmp(entries[0].inherit_from, e.inherit_from, 20) != 0 ||
        entries[0].duration != e.duration) {
        free(entries);
        cleanup_test_dir();
        return 1;
    }

    free(entries);
    cleanup_test_dir();
    return 0;
}

/* A4.13: entry_multi */
static int
test_entry_multi(void)
{
    setup_test_dir();

    tbatch_entry_t e;
    int i, err;

    for (i = 0; i < 5; i++) {
        memset(&e, 0, sizeof(e));
        memset(e.tbatch_id, (unsigned char)i, 20);
        e.start_utc_bits = (tpb_dtbits_t)(1000 + i);
        e.nkernel = (uint32_t)i;
        e.ntask = (uint32_t)(i * 2);
        e.batch_type = TPB_BATCH_TYPE_RUN;

        err = tpb_raf_entry_append_tbatch(g_test_dir, &e);
        if (err) { cleanup_test_dir(); return 1; }
    }

    tbatch_entry_t *entries = NULL;
    int count = 0;
    err = tpb_raf_entry_list_tbatch(g_test_dir, &entries,
                                      &count);
    if (err || count != 5) {
        free(entries);
        cleanup_test_dir();
        return 1;
    }

    for (i = 0; i < 5; i++) {
        if (entries[i].nkernel != (uint32_t)i ||
            entries[i].ntask != (uint32_t)(i * 2)) {
            free(entries);
            cleanup_test_dir();
            return 1;
        }
    }

    free(entries);
    cleanup_test_dir();
    return 0;
}

/* Helper: create a simple header for testing */
static tpb_meta_header_t
make_test_header(const char *name, uint32_t ndim,
                 uint64_t data_size)
{
    tpb_meta_header_t h;
    uint32_t j;

    memset(&h, 0, sizeof(h));
    h.ndim = ndim;
    h.data_size = data_size;
    h.type_bits = 0x0000083e; /* TPB_UINT64_T */
    h.uattr_bits = TPB_UNIT_NS;
    snprintf(h.name, TPBM_NAME_STR_MAX_LEN, "%s", name);
    snprintf(h.note, TPBM_NOTE_STR_MAX_LEN, "Test header %s", name);
    h.block_size = TPB_RAF_HDR_FIXED_SIZE;

    for (j = 0; j < ndim; j++) {
        snprintf(h.dimnames[j], 64, "dim%u", j);
        h.dimsizes[j] = (j + 1) * 4;
    }

    return h;
}

static void
free_test_header(tpb_meta_header_t *h)
{
    (void)h;
}

/* A4.14: record_tbatch */
static int
test_record_tbatch(void)
{
    setup_test_dir();

    tbatch_attr_t attr;
    memset(&attr, 0, sizeof(attr));
    memset(attr.tbatch_id, 0x11, 20);
    memset(attr.derive_to, 0x22, 20);
    memset(attr.inherit_from, 0x33, 20);
    attr.utc_bits = 54321;
    attr.btime = 99999;
    attr.duration = 2000000000ULL;
    snprintf(attr.hostname, 64, "node01");
    snprintf(attr.username, 64, "testuser");
    attr.front_pid = 12345;
    attr.nkernel = 1;
    attr.ntask = 2;
    attr.nscore = 0;
    attr.batch_type = TPB_BATCH_TYPE_RUN;
    attr.nheader = 1;

    tpb_meta_header_t h = make_test_header("KernelIDs", 1, 20);
    attr.headers = &h;

    unsigned char data[20];
    memset(data, 0xFF, 20);

    int err = tpb_raf_record_write_tbatch(g_test_dir, &attr,
                                            data, 20);
    free_test_header(&h);
    if (err) { cleanup_test_dir(); return 1; }

    tbatch_attr_t rattr;
    void *rdata = NULL;
    uint64_t rsize = 0;
    err = tpb_raf_record_read_tbatch(g_test_dir,
                                       attr.tbatch_id,
                                       &rattr, &rdata, &rsize);
    if (err) { cleanup_test_dir(); return 1; }

    int fail = 0;
    if (memcmp(rattr.tbatch_id, attr.tbatch_id, 20) != 0) fail = 1;
    if (memcmp(rattr.derive_to, attr.derive_to, 20) != 0) fail = 1;
    if (memcmp(rattr.inherit_from, attr.inherit_from, 20) != 0) fail = 1;
    if (rattr.utc_bits != attr.utc_bits) fail = 1;
    if (rattr.duration != attr.duration) fail = 1;
    if (rattr.nkernel != 1 || rattr.ntask != 2) fail = 1;
    if (rattr.batch_type != TPB_BATCH_TYPE_RUN) fail = 1;
    if (rattr.nheader != 1) fail = 1;
    if (rsize != 20) fail = 1;
    if (rdata && memcmp(rdata, data, 20) != 0) fail = 1;
    if (rattr.nheader < 1 || rattr.headers[0].ndim != 1) fail = 1;
    if (rattr.nheader >= 1 &&
        strcmp(rattr.headers[0].name, "KernelIDs") != 0) {
        fail = 1;
    }

    tpb_raf_free_headers(rattr.headers, rattr.nheader);
    free(rdata);
    cleanup_test_dir();
    return fail;
}

/* A4.15: record_kernel */
static int
test_record_kernel(void)
{
    setup_test_dir();

    kernel_attr_t attr;
    memset(&attr, 0, sizeof(attr));
    memset(attr.kernel_id, 0x22, 20);
    memset(attr.derive_to, 0xAA, 20);
    memset(attr.inherit_from, 0xBB, 20);
    snprintf(attr.kernel_name, 256, "triad");
    snprintf(attr.version, 64, "1.0");
    snprintf(attr.description, 2048, "Test kernel");
    attr.narg = 2;
    attr.nmetric = 1;
    attr.kctrl = TPB_KTYPE_PLI;
    attr.nheader = 1;
    attr.utc_bits = 54321;

    tpb_meta_header_t h = make_test_header("ntest", 1, 8);
    attr.headers = &h;

    uint64_t data = 100;
    int err = tpb_raf_record_write_kernel(g_test_dir, &attr,
                                            &data, 8);
    free_test_header(&h);
    if (err) { cleanup_test_dir(); return 1; }

    kernel_attr_t rattr;
    void *rdata = NULL;
    uint64_t rsize = 0;
    err = tpb_raf_record_read_kernel(g_test_dir,
                                       attr.kernel_id,
                                       &rattr, &rdata, &rsize);
    if (err) { cleanup_test_dir(); return 1; }

    int fail = 0;
    if (strcmp(rattr.kernel_name, "triad") != 0) fail = 1;
    if (memcmp(rattr.derive_to, attr.derive_to, 20) != 0) fail = 1;
    if (memcmp(rattr.inherit_from, attr.inherit_from, 20) != 0) fail = 1;
    if (rattr.narg != 2 || rattr.nmetric != 1) fail = 1;
    if (rattr.utc_bits != attr.utc_bits) fail = 1;
    if (rsize != 8) fail = 1;
    if (rdata && *(uint64_t *)rdata != 100) fail = 1;

    tpb_raf_free_headers(rattr.headers, rattr.nheader);
    free(rdata);
    cleanup_test_dir();
    return fail;
}

/* A4.16: record_task */
static int
test_record_task(void)
{
    setup_test_dir();

    task_attr_t attr;
    memset(&attr, 0, sizeof(attr));
    memset(attr.task_record_id, 0x55, 20);
    memset(attr.derive_to, 0x66, 20);
    memset(attr.inherit_from, 0x77, 20);
    memset(attr.tbatch_id, 0x11, 20);
    memset(attr.kernel_id, 0x22, 20);
    attr.utc_bits = 77777;
    attr.btime = 88888;
    attr.duration = 500000000ULL;
    attr.exit_code = 0;
    attr.handle_index = 0;
    attr.pid = 4321;
    attr.tid = 8765;
    attr.ninput = 1;
    attr.noutput = 1;
    attr.nheader = 1;

    tpb_meta_header_t h = make_test_header("elapsed", 1, 8);
    attr.headers = &h;

    int64_t elapsed = 123456789LL;
    int err = tpb_raf_record_write_task(g_test_dir, &attr,
                                          &elapsed, 8);
    free_test_header(&h);
    if (err) { cleanup_test_dir(); return 1; }

    task_attr_t rattr;
    void *rdata = NULL;
    uint64_t rsize = 0;
    err = tpb_raf_record_read_task(g_test_dir,
                                     attr.task_record_id,
                                     &rattr, &rdata, &rsize);
    if (err) { cleanup_test_dir(); return 1; }

    int fail = 0;
    if (memcmp(rattr.derive_to, attr.derive_to, 20) != 0) fail = 1;
    if (memcmp(rattr.inherit_from, attr.inherit_from, 20) != 0) fail = 1;
    if (rattr.pid != attr.pid) fail = 1;
    if (rattr.tid != attr.tid) fail = 1;
    if (rattr.duration != 500000000ULL) fail = 1;
    if (rattr.ninput != 1 || rattr.noutput != 1) fail = 1;
    if (rsize != 8) fail = 1;
    if (rdata && *(int64_t *)rdata != 123456789LL) fail = 1;

    tpb_raf_free_headers(rattr.headers, rattr.nheader);
    free(rdata);
    cleanup_test_dir();
    return fail;
}

/* A4.17: header_1d */
static int
test_header_1d(void)
{
    setup_test_dir();

    tbatch_attr_t attr;
    memset(&attr, 0, sizeof(attr));
    memset(attr.tbatch_id, 0x71, 20);
    attr.nheader = 1;

    tpb_meta_header_t h = make_test_header("single", 1, 32);
    h.dimsizes[0] = 4;
    snprintf(h.dimnames[0], 64, "elements");
    attr.headers = &h;

    uint64_t data[4] = {10, 20, 30, 40};
    int err = tpb_raf_record_write_tbatch(g_test_dir, &attr,
                                            data, 32);
    free_test_header(&h);
    if (err) { cleanup_test_dir(); return 1; }

    tbatch_attr_t rattr;
    void *rdata = NULL;
    uint64_t rsize = 0;
    err = tpb_raf_record_read_tbatch(g_test_dir,
                                       attr.tbatch_id,
                                       &rattr, &rdata, &rsize);
    if (err) { cleanup_test_dir(); return 1; }

    int fail = 0;
    if (rattr.nheader < 1 || rattr.headers[0].ndim != 1) fail = 1;
    if (rattr.nheader >= 1 && rattr.headers[0].dimsizes[0] != 4) fail = 1;
    if (rattr.nheader >= 1 &&
        strcmp(rattr.headers[0].dimnames[0], "elements") != 0) {
        fail = 1;
    }
    if (rattr.nheader >= 1 && rattr.headers[0].type_bits != 0x0000083e) fail = 1;
    if (rattr.nheader >= 1 && rattr.headers[0].uattr_bits != TPB_UNIT_NS) fail = 1;
    if (rsize != 32) fail = 1;

    tpb_raf_free_headers(rattr.headers, rattr.nheader);
    free(rdata);
    cleanup_test_dir();
    return fail;
}

/* A4.18: header_multidim */
static int
test_header_multidim(void)
{
    setup_test_dir();

    tbatch_attr_t attr;
    memset(&attr, 0, sizeof(attr));
    memset(attr.tbatch_id, 0x72, 20);
    attr.nheader = 1;

    tpb_meta_header_t h = make_test_header("matrix", 3, 96);
    h.dimsizes[0] = 2;
    h.dimsizes[1] = 3;
    h.dimsizes[2] = 4;
    snprintf(h.dimnames[0], 64, "cols");
    snprintf(h.dimnames[1], 64, "rows");
    snprintf(h.dimnames[2], 64, "layers");
    attr.headers = &h;

    uint64_t data[12];
    int k;
    for (k = 0; k < 12; k++) data[k] = (uint64_t)(k + 1);

    int err = tpb_raf_record_write_tbatch(g_test_dir, &attr,
                                            data, 96);
    free_test_header(&h);
    if (err) { cleanup_test_dir(); return 1; }

    tbatch_attr_t rattr;
    void *rdata = NULL;
    uint64_t rsize = 0;
    err = tpb_raf_record_read_tbatch(g_test_dir,
                                       attr.tbatch_id,
                                       &rattr, &rdata, &rsize);
    if (err) { cleanup_test_dir(); return 1; }

    int fail = 0;
    if (rattr.nheader < 1 || rattr.headers[0].ndim != 3) fail = 1;
    if (rattr.nheader >= 1 && rattr.headers[0].dimsizes[0] != 2) fail = 1;
    if (rattr.nheader >= 1 && rattr.headers[0].dimsizes[1] != 3) fail = 1;
    if (rattr.nheader >= 1 && rattr.headers[0].dimsizes[2] != 4) fail = 1;
    if (rattr.nheader >= 1 &&
        strcmp(rattr.headers[0].dimnames[2], "layers") != 0) {
        fail = 1;
    }

    tpb_raf_free_headers(rattr.headers, rattr.nheader);
    free(rdata);
    cleanup_test_dir();
    return fail;
}

/* A4.19: header_mixed */
static int
test_header_mixed(void)
{
    setup_test_dir();

    tbatch_attr_t attr;
    memset(&attr, 0, sizeof(attr));
    memset(attr.tbatch_id, 0x73, 20);
    attr.nkernel = 2;
    attr.ntask = 3;

    tpb_meta_header_t hdrs[3];
    hdrs[0] = make_test_header("KernelIDs", 1, 40);
    hdrs[0].dimsizes[0] = 2;
    hdrs[1] = make_test_header("TaskIDs", 1, 60);
    hdrs[1].dimsizes[0] = 3;
    hdrs[2] = make_test_header("custom_metric", 2, 48);
    hdrs[2].dimsizes[0] = 4;
    hdrs[2].dimsizes[1] = 6;

    attr.nheader = 3;
    attr.headers = hdrs;

    unsigned char data[148];
    memset(data, 0x42, sizeof(data));

    int err = tpb_raf_record_write_tbatch(g_test_dir, &attr,
                                            data, 148);
    free_test_header(&hdrs[0]);
    free_test_header(&hdrs[1]);
    free_test_header(&hdrs[2]);
    if (err) { cleanup_test_dir(); return 1; }

    tbatch_attr_t rattr;
    void *rdata = NULL;
    uint64_t rsize = 0;
    err = tpb_raf_record_read_tbatch(g_test_dir,
                                       attr.tbatch_id,
                                       &rattr, &rdata, &rsize);
    if (err) { cleanup_test_dir(); return 1; }

    int fail = 0;
    if (rattr.nheader != 3) fail = 1;
    if (strcmp(rattr.headers[0].name,
              "KernelIDs") != 0) {
        fail = 1;
    }
    if (strcmp(rattr.headers[1].name,
              "TaskIDs") != 0) {
        fail = 1;
    }
    if (strcmp(rattr.headers[2].name,
              "custom_metric") != 0) {
        fail = 1;
    }
    if (rattr.nheader < 3 || rattr.headers[2].ndim != 2) fail = 1;
    if (rattr.nheader >= 3 && rattr.headers[2].dimsizes[0] != 4) fail = 1;
    if (rattr.nheader >= 3 && rattr.headers[2].dimsizes[1] != 6) fail = 1;
    if (rsize != 148) fail = 1;
    if (rdata) {
        unsigned char *rp = (unsigned char *)rdata;
        if (rp[0] != 0x42 || rp[147] != 0x42) fail = 1;
    }

    tpb_raf_free_headers(rattr.headers, rattr.nheader);
    free(rdata);
    cleanup_test_dir();
    return fail;
}

/* A4.20: rtenv_domain_dir */
static int
test_rtenv_domain_dir(void)
{
    char path[600];
    struct stat st;

    setup_test_dir();
    snprintf(path, sizeof(path), "%s/%s", g_test_dir, TPB_RAF_RTENV_DIR);
    if (stat(path, &st) != 0 || !S_ISDIR(st.st_mode)) {
        cleanup_test_dir();
        return 1;
    }
    cleanup_test_dir();
    return 0;
}

static tpb_raf_rtenv_entry_t
make_rtenv_entry(int32_t id, const char *name)
{
    tpb_raf_rtenv_entry_t e;

    memset(&e, 0, sizeof(e));
    e.id = id;
    snprintf(e.name, sizeof(e.name), "%s", name);
    snprintf(e.hostname, sizeof(e.hostname), "testhost");
    e.utc_bits = 1000;
    e.inherit_from = 0;
    e.derive_to = -1;
    snprintf(e.note, sizeof(e.note), "note-%d", id);
    return e;
}

/* A4.21: rtenv_id_alloc */
static int
test_rtenv_id_alloc(void)
{
    tpb_raf_rtenv_entry_t e;
    int32_t next_id;
    int err;

    setup_test_dir();

    err = tpb_raf_rtenv_alloc_next_id(g_test_dir, &next_id);
    if (err || next_id != 1) {
        cleanup_test_dir();
        return 1;
    }

    e = make_rtenv_entry(1, "base");
    err = tpb_raf_entry_append_rtenv(g_test_dir, &e);
    if (err) {
        cleanup_test_dir();
        return 1;
    }

    err = tpb_raf_rtenv_alloc_next_id(g_test_dir, &next_id);
    if (err || next_id != 2) {
        cleanup_test_dir();
        return 1;
    }

    e = make_rtenv_entry(2, "child");
    err = tpb_raf_entry_append_rtenv(g_test_dir, &e);
    if (err) {
        cleanup_test_dir();
        return 1;
    }

    err = tpb_raf_rtenv_alloc_next_id(g_test_dir, &next_id);
    if (err || next_id != 3) {
        cleanup_test_dir();
        return 1;
    }

    cleanup_test_dir();
    return 0;
}

/* A4.22: rtenv_dup_name */
static int
test_rtenv_dup_name(void)
{
    tpb_raf_rtenv_entry_t e;
    int err;

    setup_test_dir();
    e = make_rtenv_entry(1, "dup-name");
    err = tpb_raf_entry_append_rtenv(g_test_dir, &e);
    if (err) {
        cleanup_test_dir();
        return 1;
    }

    e.id = 2;
    err = tpb_raf_entry_append_rtenv(g_test_dir, &e);
    if (err == TPBE_SUCCESS || TPBE_CAUSE(err) != TPBE_LIST_DUP) {
        cleanup_test_dir();
        return 1;
    }

    cleanup_test_dir();
    return 0;
}

/* A4.23: rtenv_entry_roundtrip */
static int
test_rtenv_entry_roundtrip(void)
{
    tpb_raf_rtenv_entry_t e;
    tpb_raf_rtenv_entry_t *entries = NULL;
    int count = 0;
    int err;

    setup_test_dir();
    e = make_rtenv_entry(1, "roundtrip");
    e.napp = 2;
    e.nenv = 3;
    err = tpb_raf_entry_append_rtenv(g_test_dir, &e);
    if (err) {
        cleanup_test_dir();
        return 1;
    }

    err = tpb_raf_entry_list_rtenv(g_test_dir, &entries, &count);
    if (err || count != 1) {
        free(entries);
        cleanup_test_dir();
        return 1;
    }

    if (entries[0].id != 1 ||
        strcmp(entries[0].name, "roundtrip") != 0 ||
        entries[0].napp != 2 ||
        entries[0].nenv != 3) {
        free(entries);
        cleanup_test_dir();
        return 1;
    }

    free(entries);
    cleanup_test_dir();
    return 0;
}

/* A4.24: rtenv_record_roundtrip */
static int
test_rtenv_record_roundtrip(void)
{
    tpb_raf_rtenv_entry_t ent;
    tpb_raf_rtenv_attr_t attr;
    tpb_meta_header_t hdrs[5];
    char app_data[192];
    char key_data[] = "PATH";
    char val_data[] = "/usr/bin";
    uint32_t on_set_data = TPB_RTENV_ON_SET_OVERWRITE;
    uint32_t on_get_data = TPB_RTENV_ON_GET_OVERWRITE;
    unsigned char payload[256];
    size_t off = 0;
    tpb_meta_header_t *rheaders = NULL;
    void *rdata = NULL;
    uint64_t rsize = 0;
    int err;
    int fail = 0;

    setup_test_dir();
    ent = make_rtenv_entry(1, "rec-test");
    ent.napp = 1;
    ent.nenv = 1;
    err = tpb_raf_entry_append_rtenv(g_test_dir, &ent);
    if (err) {
        cleanup_test_dir();
        return 1;
    }

    memset(&attr, 0, sizeof(attr));
    attr.id = 1;
    snprintf(attr.name, sizeof(attr.name), "%s", ent.name);
    snprintf(attr.hostname, sizeof(attr.hostname), "%s", ent.hostname);
    attr.utc_bits = ent.utc_bits;
    attr.inherit_from = ent.inherit_from;
    attr.napp = 1;
    attr.nenv = 1;
    attr.nheader = 5;

    memset(app_data, 0, sizeof(app_data));
    snprintf(app_data, 64, "gcc");
    snprintf(app_data + 64, 64, "13.2");
    snprintf(app_data + 128, 64, "compiler");

    memset(hdrs, 0, sizeof(hdrs));
    hdrs[0].ndim = 3;
    hdrs[0].dimsizes[0] = 64;
    hdrs[0].dimsizes[1] = 3;
    hdrs[0].dimsizes[2] = 1;
    hdrs[0].data_size = sizeof(app_data);
    hdrs[0].type_bits = (uint32_t)(TPB_STRING_T & TPB_PARM_TYPE_MASK);
    hdrs[0].block_size = TPB_RAF_HDR_FIXED_SIZE;
    snprintf(hdrs[0].name, TPBM_NAME_STR_MAX_LEN, "application");

    hdrs[1].ndim = 1;
    hdrs[1].dimsizes[0] = sizeof(key_data);
    hdrs[1].data_size = sizeof(key_data);
    hdrs[1].type_bits = (uint32_t)(TPB_STRING_T & TPB_PARM_TYPE_MASK);
    hdrs[1].block_size = TPB_RAF_HDR_FIXED_SIZE;
    snprintf(hdrs[1].name, TPBM_NAME_STR_MAX_LEN, "key[0]");

    hdrs[2].ndim = 1;
    hdrs[2].dimsizes[0] = sizeof(val_data);
    hdrs[2].data_size = sizeof(val_data);
    hdrs[2].type_bits = (uint32_t)(TPB_STRING_T & TPB_PARM_TYPE_MASK);
    hdrs[2].block_size = TPB_RAF_HDR_FIXED_SIZE;
    snprintf(hdrs[2].name, TPBM_NAME_STR_MAX_LEN, "value[0]");

    hdrs[3].ndim = 1;
    hdrs[3].dimsizes[0] = sizeof(uint32_t);
    hdrs[3].data_size = sizeof(uint32_t);
    hdrs[3].type_bits = (uint32_t)(TPB_UINT32_T & TPB_PARM_TYPE_MASK);
    hdrs[3].block_size = TPB_RAF_HDR_FIXED_SIZE;
    snprintf(hdrs[3].name, TPBM_NAME_STR_MAX_LEN, "on_set[0]");

    hdrs[4].ndim = 1;
    hdrs[4].dimsizes[0] = sizeof(uint32_t);
    hdrs[4].data_size = sizeof(uint32_t);
    hdrs[4].type_bits = (uint32_t)(TPB_UINT32_T & TPB_PARM_TYPE_MASK);
    hdrs[4].block_size = TPB_RAF_HDR_FIXED_SIZE;
    snprintf(hdrs[4].name, TPBM_NAME_STR_MAX_LEN, "on_get[0]");

    memcpy(payload + off, app_data, sizeof(app_data));
    off += sizeof(app_data);
    memcpy(payload + off, key_data, sizeof(key_data));
    off += sizeof(key_data);
    memcpy(payload + off, val_data, sizeof(val_data));
    off += sizeof(val_data);
    memcpy(payload + off, &on_set_data, sizeof(on_set_data));
    off += sizeof(on_set_data);
    memcpy(payload + off, &on_get_data, sizeof(on_get_data));
    off += sizeof(on_get_data);

    err = tpb_raf_record_write_rtenv(g_test_dir, &attr, hdrs,
                                     payload, (uint64_t)off);
    if (err) {
        cleanup_test_dir();
        return 1;
    }

    err = tpb_raf_record_read_rtenv(g_test_dir, 1, &attr,
                                    &rheaders, &rdata, &rsize);
    if (err) {
        cleanup_test_dir();
        return 1;
    }

    if (attr.nheader != 5 || attr.napp != 1 || attr.nenv != 1) {
        fail = 1;
    }
    if (strcmp(rheaders[0].name, "application") != 0) {
        fail = 1;
    }
    if (strcmp(rheaders[1].name, "key[0]") != 0) {
        fail = 1;
    }
    if (strcmp(rheaders[2].name, "value[0]") != 0) {
        fail = 1;
    }
    if (strcmp(rheaders[3].name, "on_set[0]") != 0) {
        fail = 1;
    }
    if (strcmp(rheaders[4].name, "on_get[0]") != 0) {
        fail = 1;
    }
    if (rsize != (uint64_t)off) {
        fail = 1;
    }

    tpb_raf_free_headers(rheaders, attr.nheader);
    free(rdata);
    cleanup_test_dir();
    return fail;
}

/* A4.25: rtenv_decimal_path */
static int
test_rtenv_decimal_path(void)
{
    tpb_raf_rtenv_attr_t attr;
    char fpath[600];
    unsigned char fake_id[20];
    uint8_t dom = 0;
    int err;

    setup_test_dir();

    memset(&attr, 0, sizeof(attr));
    attr.id = 1;
    snprintf(attr.name, sizeof(attr.name), "path-test");
    attr.nheader = 0;

    err = tpb_raf_record_write_rtenv(g_test_dir, &attr, NULL, NULL, 0);
    if (err) {
        cleanup_test_dir();
        return 1;
    }

    snprintf(fpath, sizeof(fpath), "%s/%s/%010d.tpbr",
             g_test_dir, TPB_RAF_RTENV_DIR, 1);
    {
        struct stat st;
        if (stat(fpath, &st) != 0 || !S_ISREG(st.st_mode)) {
            cleanup_test_dir();
            return 1;
        }
    }

    {
        uint8_t ftype = 0;
        uint8_t domain = 0;
        err = tpb_raf_detect_file(fpath, &ftype, &domain);
        if (err || ftype != TPB_RAF_FTYPE_RECORD ||
            domain != TPB_RAF_DOM_RTENV) {
            cleanup_test_dir();
            return 1;
        }
    }

    memset(fake_id, 0xAB, 20);
    err = tpb_raf_find_record(g_test_dir, fake_id, &dom);
    if (err == TPBE_SUCCESS && dom == TPB_RAF_DOM_RTENV) {
        cleanup_test_dir();
        return 1;
    }

    cleanup_test_dir();
    return 0;
}

static int
write_min_rtenv_record(int32_t id, const char *name)
{
    tpb_raf_rtenv_attr_t attr;

    memset(&attr, 0, sizeof(attr));
    attr.id = id;
    snprintf(attr.name, sizeof(attr.name), "%s", name);
    attr.nheader = 0;
    return tpb_raf_record_write_rtenv(g_test_dir, &attr, NULL, NULL, 0);
}

/* A4.26: rtenv_counter_patch */
static int
test_rtenv_counter_patch(void)
{
    tpb_raf_rtenv_entry_t e;
    tpb_raf_rtenv_entry_t *entries = NULL;
    tpb_raf_rtenv_attr_t attr;
    int count = 0;
    int err;

    setup_test_dir();
    e = make_rtenv_entry(1, "ctr");
    err = tpb_raf_entry_append_rtenv(g_test_dir, &e);
    if (err) {
        cleanup_test_dir();
        return 1;
    }
    err = write_min_rtenv_record(1, "ctr");
    if (err) {
        cleanup_test_dir();
        return 1;
    }

    err = tpb_raf_record_patch_rtenv_counters(g_test_dir, 1, 2, 1);
    if (err) {
        cleanup_test_dir();
        return 1;
    }

    err = tpb_raf_entry_list_rtenv(g_test_dir, &entries, &count);
    if (err || count != 1 || entries[0].ntask != 2 ||
        entries[0].ntbatch != 1) {
        free(entries);
        cleanup_test_dir();
        return 1;
    }
    free(entries);

    {
        tpb_meta_header_t *hdrs = NULL;
        void *data = NULL;
        uint64_t dsize = 0;

        err = tpb_raf_record_read_rtenv(g_test_dir, 1, &attr,
                                        &hdrs, &data, &dsize);
        if (err || attr.ntask != 2 || attr.ntbatch != 1) {
            free(data);
            tpb_raf_free_headers(hdrs, attr.nheader);
            cleanup_test_dir();
            return 1;
        }
        free(data);
        tpb_raf_free_headers(hdrs, attr.nheader);
    }

    cleanup_test_dir();
    return 0;
}

/* A4.27: rtenv_derive_link */
static int
test_rtenv_derive_link(void)
{
    tpb_raf_rtenv_entry_t pe;
    tpb_raf_rtenv_entry_t ce;
    tpb_raf_rtenv_attr_t attr;
    tpb_meta_header_t *hdrs = NULL;
    void *data = NULL;
    uint64_t dsize = 0;
    int derive_idx;
    int err;

    setup_test_dir();
    pe = make_rtenv_entry(1, "parent");
    ce = make_rtenv_entry(2, "child1");
    if (tpb_raf_entry_append_rtenv(g_test_dir, &pe) != TPBE_SUCCESS ||
        tpb_raf_entry_append_rtenv(g_test_dir, &ce) != TPBE_SUCCESS) {
        cleanup_test_dir();
        return 1;
    }
    if (write_min_rtenv_record(1, "parent") != TPBE_SUCCESS) {
        cleanup_test_dir();
        return 1;
    }

    err = tpb_raf_record_append_rtenv_derive(g_test_dir, 1, 2);
    if (err) {
        cleanup_test_dir();
        return 1;
    }

    err = tpb_raf_record_read_rtenv(g_test_dir, 1, &attr, &hdrs, &data, &dsize);
    if (err || attr.derive_to != 2) {
        free(data);
        tpb_raf_free_headers(hdrs, attr.nheader);
        cleanup_test_dir();
        return 1;
    }
    derive_idx = -1;
    {
        uint32_t i;
        for (i = 0; i < attr.nheader; i++) {
            if (strcmp(hdrs[i].name, "DeriveTo") == 0) {
                derive_idx = (int)i;
                break;
            }
        }
    }
    if (derive_idx < 0 || hdrs[derive_idx].dimsizes[0] != 1) {
        free(data);
        tpb_raf_free_headers(hdrs, attr.nheader);
        cleanup_test_dir();
        return 1;
    }
    {
        const int32_t *ids = (const int32_t *)data;
        if (ids[0] != 2) {
            free(data);
            tpb_raf_free_headers(hdrs, attr.nheader);
            cleanup_test_dir();
            return 1;
        }
    }
    free(data);
    tpb_raf_free_headers(hdrs, attr.nheader);

    ce = make_rtenv_entry(3, "child2");
    if (tpb_raf_entry_append_rtenv(g_test_dir, &ce) != TPBE_SUCCESS) {
        cleanup_test_dir();
        return 1;
    }
    err = tpb_raf_record_append_rtenv_derive(g_test_dir, 1, 3);
    if (err) {
        cleanup_test_dir();
        return 1;
    }

    err = tpb_raf_record_read_rtenv(g_test_dir, 1, &attr, &hdrs, &data, &dsize);
    if (err || attr.derive_to != 3) {
        free(data);
        tpb_raf_free_headers(hdrs, attr.nheader);
        cleanup_test_dir();
        return 1;
    }
    derive_idx = -1;
    {
        uint32_t i;
        for (i = 0; i < attr.nheader; i++) {
            if (strcmp(hdrs[i].name, "DeriveTo") == 0) {
                derive_idx = (int)i;
                break;
            }
        }
    }
    if (derive_idx < 0 || hdrs[derive_idx].dimsizes[0] != 2) {
        free(data);
        tpb_raf_free_headers(hdrs, attr.nheader);
        cleanup_test_dir();
        return 1;
    }
    {
        const int32_t *ids = (const int32_t *)data;
        if (ids[0] != 2 || ids[1] != 3) {
            free(data);
            tpb_raf_free_headers(hdrs, attr.nheader);
            cleanup_test_dir();
            return 1;
        }
    }
    free(data);
    tpb_raf_free_headers(hdrs, attr.nheader);
    cleanup_test_dir();
    return 0;
}

/* A4.28: rtenv_config_base_id */
static int
test_rtenv_config_base_id(void)
{
    char cfgpath[600];
    char line[512];
    FILE *fp;
    int32_t base_id = -1;
    int has_name = 0;
    int has_base5 = 0;

    setup_test_dir();
    if (tpb_raf_config_get_base_id(g_test_dir, &base_id) != TPBE_SUCCESS ||
        base_id != 1) {
        cleanup_test_dir();
        return 1;
    }

    if (tpb_raf_config_set_base_id(g_test_dir, 5) != TPBE_SUCCESS) {
        cleanup_test_dir();
        return 1;
    }
    base_id = -1;
    if (tpb_raf_config_get_base_id(g_test_dir, &base_id) != TPBE_SUCCESS ||
        base_id != 5) {
        cleanup_test_dir();
        return 1;
    }

    snprintf(cfgpath, sizeof(cfgpath), "%s/%s",
             g_test_dir, TPB_RAF_CONFIG_REL);
    fp = fopen(cfgpath, "r");
    if (!fp) {
        cleanup_test_dir();
        return 1;
    }
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strstr(line, "\"name\"") != NULL &&
            strstr(line, "default") != NULL) {
            has_name = 1;
        }
        if (strstr(line, "base_id") != NULL && strstr(line, "5") != NULL) {
            has_base5 = 1;
        }
    }
    fclose(fp);
    cleanup_test_dir();
    return (has_name && has_base5) ? 0 : 1;
}

/* A4.29: rtenv_resolve_active */
static int
test_rtenv_resolve_active(void)
{
    tpb_raf_rtenv_entry_t e;
    int32_t active = -1;
    int err;

    setup_test_dir();
    e = make_rtenv_entry(1, "active0");
    if (tpb_raf_entry_append_rtenv(g_test_dir, &e) != TPBE_SUCCESS) {
        cleanup_test_dir();
        return 1;
    }

    if (setenv("TPB_RTENV_ID", "1", 1) != 0) {
        cleanup_test_dir();
        return 1;
    }
    err = tpb_rtenv_resolve_active_id(g_test_dir, &active);
    unsetenv("TPB_RTENV_ID");
    if (err != TPBE_SUCCESS || active != 1) {
        cleanup_test_dir();
        return 1;
    }

    if (tpb_raf_config_set_base_id(g_test_dir, 1) != TPBE_SUCCESS) {
        cleanup_test_dir();
        return 1;
    }
    unsetenv("TPB_RTENV_ID");
    active = -1;
    err = tpb_rtenv_resolve_active_id(g_test_dir, &active);
    if (err != TPBE_SUCCESS || active != 1) {
        cleanup_test_dir();
        return 1;
    }

    cleanup_test_dir();
    setup_test_dir();
    active = -1;
    err = tpb_rtenv_resolve_active_id(g_test_dir, &active);
    if (err != TPBE_SUCCESS || active != 1) {
        cleanup_test_dir();
        return 1;
    }
    {
        tpb_raf_rtenv_entry_t *entries = NULL;
        int count = 0;
        err = tpb_raf_entry_list_rtenv(g_test_dir, &entries, &count);
        if (err != TPBE_SUCCESS || count != 1 || entries[0].id != 1) {
            free(entries);
            cleanup_test_dir();
            return 1;
        }
        free(entries);
    }

    cleanup_test_dir();
    return 0;
}

/* A4.30: rtenv_build_snapshot */
static int
test_rtenv_build_snapshot(void)
{
    int32_t id = -1;
    int32_t base_id = -1;
    tpb_raf_rtenv_entry_t *entries = NULL;
    int count = 0;
    int err;

    setup_test_dir();
    err = tpb_rtenv_snapshot_build_env(g_test_dir, 1, &id);
    if (err != TPBE_SUCCESS || id != 1) {
        cleanup_test_dir();
        return 1;
    }

    err = tpb_raf_entry_list_rtenv(g_test_dir, &entries, &count);
    if (err != TPBE_SUCCESS || count != 1 || entries[0].id != 1 ||
        entries[0].inherit_from != 0 || entries[0].nenv == 0 ||
        strcmp(entries[0].name, "base") != 0 ||
        entries[0].utc_bits == 0 ||
        strcmp(entries[0].hostname, "localhost") == 0) {
        free(entries);
        cleanup_test_dir();
        return 1;
    }
    free(entries);

    err = tpb_raf_config_get_base_id(g_test_dir, &base_id);
    if (err != TPBE_SUCCESS || base_id != 1) {
        cleanup_test_dir();
        return 1;
    }

    id = -1;
    err = tpb_rtenv_snapshot_build_env(g_test_dir, 1, &id);
    if (err != TPBE_SUCCESS || id != 2) {
        cleanup_test_dir();
        return 1;
    }

    err = tpb_raf_entry_list_rtenv(g_test_dir, &entries, &count);
    if (err != TPBE_SUCCESS || count != 2 || strcmp(entries[1].name, "base_1") != 0) {
        free(entries);
        cleanup_test_dir();
        return 1;
    }
    free(entries);

    err = tpb_raf_config_get_base_id(g_test_dir, &base_id);
    if (err != TPBE_SUCCESS || base_id != 2) {
        cleanup_test_dir();
        return 1;
    }

    cleanup_test_dir();
    return 0;
}

int
main(int argc, char **argv)
{
    const char *filter = (argc > 1) ? argv[1] : NULL;
    test_case_t cases[] = {
        { "A4.1",  "magic_construct",    test_magic_construct },
        { "A4.2",  "magic_validate_ok",  test_magic_validate_ok },
        { "A4.3",  "magic_validate_bad", test_magic_validate_bad },
        { "A4.6",  "id_tbatch",          test_id_tbatch },
        { "A4.7",  "id_kernel",          test_id_kernel },
        { "A4.8",  "id_task",            test_id_task },
        { "A4.9",  "id_uniqueness",      test_id_uniqueness },
        { "A4.10", "entry_tbatch",       test_entry_tbatch },
        { "A4.11", "entry_kernel",       test_entry_kernel },
        { "A4.12", "entry_task",         test_entry_task },
        { "A4.13", "entry_multi",        test_entry_multi },
        { "A4.14", "record_tbatch",      test_record_tbatch },
        { "A4.15", "record_kernel",      test_record_kernel },
        { "A4.16", "record_task",        test_record_task },
        { "A4.17", "header_1d",          test_header_1d },
        { "A4.18", "header_multidim",    test_header_multidim },
        { "A4.19", "header_mixed",       test_header_mixed },
        { "A4.20", "rtenv_domain_dir",   test_rtenv_domain_dir },
        { "A4.21", "rtenv_id_alloc",     test_rtenv_id_alloc },
        { "A4.22", "rtenv_dup_name",     test_rtenv_dup_name },
        { "A4.23", "rtenv_entry_roundtrip", test_rtenv_entry_roundtrip },
        { "A4.24", "rtenv_record_roundtrip", test_rtenv_record_roundtrip },
        { "A4.25", "rtenv_decimal_path", test_rtenv_decimal_path },
        { "A4.26", "rtenv_counter_patch", test_rtenv_counter_patch },
        { "A4.27", "rtenv_derive_link",   test_rtenv_derive_link },
        { "A4.28", "rtenv_config_base_id", test_rtenv_config_base_id },
        { "A4.29", "rtenv_resolve_active", test_rtenv_resolve_active },
        { "A4.30", "rtenv_build_snapshot", test_rtenv_build_snapshot },
    };
    int n = sizeof(cases) / sizeof(cases[0]);
    int fail = run_pack("A4", cases, n, filter);
    return (fail > 0) ? 1 : 0;
}
