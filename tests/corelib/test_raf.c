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
static int test_magic_scan_tpbe(void);
static int test_magic_scan_tpbr(void);
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

/* A4.4: magic_scan_tpbe */
static int
test_magic_scan_tpbe(void)
{
    unsigned char buf[256];
    unsigned char magic[8];
    size_t offsets[4];
    int nfound = 0;

    memset(buf, 0xAB, sizeof(buf));

    tpb_raf_build_magic(TPB_RAF_FTYPE_ENTRY,
                          TPB_RAF_DOM_TBATCH,
                          TPB_RAF_POS_START, magic);
    memcpy(buf + 50, magic, 8);
    memcpy(buf + 200, magic, 8);

    tpb_raf_magic_scan(buf, sizeof(buf), offsets,
                         &nfound, 4);
    if (nfound != 2) return 1;
    if (offsets[0] != 50 || offsets[1] != 200) return 1;
    return 0;
}

/* A4.5: magic_scan_tpbr */
static int
test_magic_scan_tpbr(void)
{
    unsigned char buf[128];
    unsigned char magic[8];
    size_t offsets[4];
    int nfound = 0;

    memset(buf, 0xCD, sizeof(buf));

    tpb_raf_build_magic(TPB_RAF_FTYPE_RECORD,
                          TPB_RAF_DOM_KERNEL,
                          TPB_RAF_POS_START, magic);
    memcpy(buf + 30, magic, 8);

    tpb_raf_magic_scan(buf, sizeof(buf), offsets,
                         &nfound, 4);
    if (nfound != 1) return 1;
    if (offsets[0] != 30) return 1;
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
    unsigned char so[20], bin[20];

    memset(so, 0xAA, 20);
    memset(bin, 0xBB, 20);

    tpb_raf_gen_kernel_id("triad", so, bin, id1);
    tpb_raf_gen_kernel_id("triad", so, bin, id2);

    if (memcmp(id1, id2, 20) != 0) return 1;
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
    unsigned char so[20], bin[20];

    memset(so, 0xAA, 20);
    memset(bin, 0xBB, 20);

    tpb_raf_gen_kernel_id("triad", so, bin, id_a);
    tpb_raf_gen_kernel_id("stream", so, bin, id_b);

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
    memset(e.so_sha1, 0xCC, 20);
    e.kctrl = TPB_KTYPE_PLI;
    e.nparm = 3;
    e.nmetric = 1;

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
        entries[0].nparm != 3 || entries[0].nmetric != 1) {
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
    memset(attr.so_sha1, 0x33, 20);
    memset(attr.bin_sha1, 0x44, 20);
    snprintf(attr.kernel_name, 256, "triad");
    snprintf(attr.version, 64, "1.0");
    snprintf(attr.description, 2048, "Test kernel");
    attr.nparm = 2;
    attr.nmetric = 1;
    attr.kctrl = TPB_KTYPE_PLI;
    attr.nheader = 1;

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
    if (rattr.nparm != 2 || rattr.nmetric != 1) fail = 1;
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

int
main(int argc, char **argv)
{
    const char *filter = (argc > 1) ? argv[1] : NULL;
    test_case_t cases[] = {
        { "A4.1",  "magic_construct",    test_magic_construct },
        { "A4.2",  "magic_validate_ok",  test_magic_validate_ok },
        { "A4.3",  "magic_validate_bad", test_magic_validate_bad },
        { "A4.4",  "magic_scan_tpbe",    test_magic_scan_tpbe },
        { "A4.5",  "magic_scan_tpbr",    test_magic_scan_tpbr },
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
    };
    int n = sizeof(cases) / sizeof(cases[0]);
    int fail = run_pack("A4", cases, n, filter);
    return (fail > 0) ? 1 : 0;
}
