/*
 * test_1d_array_write.c
 * Test pack A5: 1-D array output write/read round-trip via tpb_k_write_task.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include "include/tpb-public.h"
#include "corelib/rafdb/tpb-raf-types.h"
#include "mock_kernel.h"
#include "mock_timer.h"

static char g_test_dir[512];

static void
setup_test_dir(const char *tag)
{
    snprintf(g_test_dir, sizeof(g_test_dir),
             "/tmp/tpb_1d_test_%s_%d", tag, (int)getpid());
    mkdir(g_test_dir, 0755);
    setenv("TPB_WORKSPACE", g_test_dir, 1);
    unsetenv("TPB_TBATCH_ID");
    unsetenv("TPB_HANDLE_INDEX");
}

static void
cleanup_test_dir(void)
{
    char cmd[600];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", g_test_dir);
    system(cmd);
    unsetenv("TPB_WORKSPACE");
}

static size_t
elem_size_for_dtype(TPB_DTYPE dtype)
{
    switch (dtype & TPB_PARM_TYPE_MASK) {
        case TPB_INT32_T & TPB_PARM_TYPE_MASK: return 4;
        case TPB_INT64_T & TPB_PARM_TYPE_MASK: return 8;
        case TPB_FLOAT_T & TPB_PARM_TYPE_MASK: return sizeof(float);
        case TPB_DOUBLE_T & TPB_PARM_TYPE_MASK: return sizeof(double);
        default: return 0;
    }
}

static int
do_write_read_test(int nout, TPB_DTYPE *dtypes, int *counts,
                   const char **names, const char *tag)
{
    int err, ret = 0;
    tpb_k_rthdl_t hdl;

    setup_test_dir(tag);
    err = tpb_raf_init_workspace(g_test_dir);
    if (err) { cleanup_test_dir(); return 1; }

    err = mock_setup_driver();
    if (err) { cleanup_test_dir(); return 1; }
    err = mock_build_handle("mock_pli_test", &hdl);
    if (err) { cleanup_test_dir(); return 1; }

    /* Populate respack with test outputs */
    hdl.respack.n = nout;
    hdl.respack.outputs = (tpb_k_output_t *)calloc(nout, sizeof(tpb_k_output_t));
    if (!hdl.respack.outputs) { cleanup_test_dir(); return 1; }

    void **orig_data = (void **)calloc(nout, sizeof(void *));
    size_t *data_sizes = (size_t *)calloc(nout, sizeof(size_t));

    for (int i = 0; i < nout; i++) {
        size_t esz = elem_size_for_dtype(dtypes[i]);
        size_t total = (size_t)counts[i] * esz;

        snprintf(hdl.respack.outputs[i].name,
                 sizeof(hdl.respack.outputs[i].name), "%s", names[i]);
        snprintf(hdl.respack.outputs[i].note,
                 sizeof(hdl.respack.outputs[i].note), "test output %d", i);
        hdl.respack.outputs[i].dtype = dtypes[i];
        hdl.respack.outputs[i].unit  = TPB_UNIT_B;
        hdl.respack.outputs[i].n     = counts[i];
        hdl.respack.outputs[i].p     = malloc(total);
        orig_data[i] = malloc(total);
        data_sizes[i] = total;

        /* Fill with pseudo-random bytes */
        unsigned int seed = (unsigned int)(42 + i);
        uint8_t *p = (uint8_t *)hdl.respack.outputs[i].p;
        for (size_t b = 0; b < total; b++) {
            p[b] = (uint8_t)(rand_r(&seed) & 0xFF);
        }
        memcpy(orig_data[i], hdl.respack.outputs[i].p, total);
    }

    /* Count existing task entries */
    task_entry_t *entries_before = NULL;
    int count_before = 0;
    tpb_raf_entry_list_task(g_test_dir, &entries_before, &count_before);
    free(entries_before);

    /* Write via public API */
    err = tpb_k_write_task(&hdl, 0);
    if (err) {
        fprintf(stderr, "  FAIL: tpb_k_write_task returned %d\n", err);
        ret = 1;
        goto done;
    }

    /* Find the new task id */
    task_entry_t *entries_after = NULL;
    int count_after = 0;
    err = tpb_raf_entry_list_task(g_test_dir, &entries_after, &count_after);
    if (err || count_after != count_before + 1) {
        fprintf(stderr, "  FAIL: expected %d task entries, got %d (err=%d)\n",
                count_before + 1, count_after, err);
        free(entries_after);
        ret = 1;
        goto done;
    }

    unsigned char new_task_id[20];
    memcpy(new_task_id, entries_after[count_after - 1].task_record_id, 20);
    free(entries_after);

    /* Read back */
    task_attr_t rattr;
    void *rdata = NULL;
    uint64_t rdatasize = 0;

    err = tpb_raf_record_read_task(g_test_dir, new_task_id,
                                     &rattr, &rdata, &rdatasize);
    if (err) {
        fprintf(stderr, "  FAIL: tpb_raf_record_read_task returned %d\n", err);
        ret = 1;
        goto done;
    }

    /* Verify counts */
    if (rattr.ninput != (uint32_t)hdl.argpack.n) {
        fprintf(stderr, "  FAIL: ninput expected %d, got %u\n",
                hdl.argpack.n, rattr.ninput);
        ret = 1;
    }
    if (rattr.noutput != (uint32_t)nout) {
        fprintf(stderr, "  FAIL: noutput expected %d, got %u\n",
                nout, rattr.noutput);
        ret = 1;
    }
    if (rattr.nheader != rattr.ninput + rattr.noutput) {
        fprintf(stderr, "  FAIL: nheader expected %u, got %u\n",
                rattr.ninput + rattr.noutput, rattr.nheader);
        ret = 1;
    }

    /* Verify output headers */
    for (int j = 0; j < nout && !ret; j++) {
        uint32_t hi = rattr.ninput + (uint32_t)j;
        if (hi >= rattr.nheader) {
            fprintf(stderr, "  FAIL: header index %u out of range\n", hi);
            ret = 1;
            break;
        }
        tpb_meta_header_t *hdr = &rattr.headers[hi];

        if (hdr->ndim != 1) {
            fprintf(stderr, "  FAIL: output[%d] ndim=%u, expected 1\n",
                    j, hdr->ndim);
            ret = 1;
        }
        if (hdr->dimsizes[0] != (uint64_t)counts[j]) {
            fprintf(stderr, "  FAIL: output[%d] dimsizes[0]=%lu, expected %d\n",
                    j, (unsigned long)hdr->dimsizes[0], counts[j]);
            ret = 1;
        }
        size_t expected_dsz = (size_t)counts[j] * elem_size_for_dtype(dtypes[j]);
        if (hdr->data_size != (uint64_t)expected_dsz) {
            fprintf(stderr, "  FAIL: output[%d] data_size=%lu, expected %zu\n",
                    j, (unsigned long)hdr->data_size, expected_dsz);
            ret = 1;
        }
        if (strcmp(hdr->name, names[j]) != 0) {
            fprintf(stderr, "  FAIL: output[%d] name='%s', expected '%s'\n",
                    j, hdr->name, names[j]);
            ret = 1;
        }
        if ((hdr->type_bits & TPB_PARM_TYPE_MASK) !=
            (uint32_t)(dtypes[j] & TPB_PARM_TYPE_MASK)) {
            fprintf(stderr, "  FAIL: output[%d] type_bits mismatch\n", j);
            ret = 1;
        }
    }

    /* Verify payload data */
    if (!ret && rdata && rdatasize > 0) {
        uint64_t offset = 0;
        for (uint32_t i = 0; i < rattr.ninput; i++) {
            offset += rattr.headers[i].data_size;
        }
        for (int j = 0; j < nout && !ret; j++) {
            size_t blksz = data_sizes[j];
            if (offset + blksz > rdatasize) {
                fprintf(stderr, "  FAIL: data too short for output[%d]\n", j);
                ret = 1;
                break;
            }
            if (memcmp((uint8_t *)rdata + offset, orig_data[j], blksz) != 0) {
                fprintf(stderr, "  FAIL: output[%d] data mismatch\n", j);
                ret = 1;
            }
            offset += blksz;
        }
    }

    tpb_raf_free_headers(rattr.headers, rattr.nheader);
    free(rdata);

done:
    for (int i = 0; i < nout; i++) {
        free(hdl.respack.outputs[i].p);
        free(orig_data[i]);
    }
    free(hdl.respack.outputs);
    hdl.respack.outputs = NULL;
    hdl.respack.n = 0;
    free(orig_data);
    free(data_sizes);
    tpb_driver_clean_handle(&hdl);
    cleanup_test_dir();
    return ret;
}

/* ---- A5.1: doubles ---- */
static int
test_write_read_doubles(void)
{
    srand(101);
    int n = 1 + (rand() % 100);
    TPB_DTYPE dt = TPB_DOUBLE_T;
    const char *nm = "bw_walltime";
    return do_write_read_test(1, &dt, &n, &nm, "dbl");
}

/* ---- A5.2: floats ---- */
static int
test_write_read_floats(void)
{
    srand(202);
    int n = 1 + (rand() % 256);
    TPB_DTYPE dt = TPB_FLOAT_T;
    const char *nm = "latency_f32";
    return do_write_read_test(1, &dt, &n, &nm, "flt");
}

/* ---- A5.3: int64 ---- */
static int
test_write_read_int64(void)
{
    srand(303);
    int n = 1 + (rand() % 50);
    TPB_DTYPE dt = TPB_INT64_T;
    const char *nm = "cycle_counts";
    return do_write_read_test(1, &dt, &n, &nm, "i64");
}

/* ---- A5.4: int32 ---- */
static int
test_write_read_int32(void)
{
    srand(404);
    int n = 1 + (rand() % 200);
    TPB_DTYPE dt = TPB_INT32_T;
    const char *nm = "error_codes";
    return do_write_read_test(1, &dt, &n, &nm, "i32");
}

/* ---- A5.5: random dtype + random length ---- */
static int
test_write_read_random_len(void)
{
    srand(505);
    TPB_DTYPE choices[] = { TPB_INT32_T, TPB_INT64_T, TPB_FLOAT_T, TPB_DOUBLE_T };
    TPB_DTYPE dt = choices[rand() % 4];
    int n = 1 + (rand() % 1000);
    const char *nm = "random_metric";
    return do_write_read_test(1, &dt, &n, &nm, "rnd");
}

/* ---- A5.6: multiple outputs ---- */
static int
test_write_read_multi(void)
{
    srand(606);
    int nout = 3;
    TPB_DTYPE dtypes[3] = { TPB_DOUBLE_T, TPB_INT32_T, TPB_FLOAT_T };
    int counts[3];
    const char *names[3] = { "throughput", "iterations", "avg_latency" };

    counts[0] = 1 + (rand() % 64);
    counts[1] = 1 + (rand() % 128);
    counts[2] = 1 + (rand() % 32);

    return do_write_read_test(nout, dtypes, counts, names, "multi");
}

/* ---- main ---- */
int
main(int argc, char **argv)
{
    const char *filter = (argc >= 2) ? argv[1] : NULL;

    test_case_t cases[] = {
        { "A5.1", "write_read_doubles",    test_write_read_doubles },
        { "A5.2", "write_read_floats",     test_write_read_floats },
        { "A5.3", "write_read_int64",      test_write_read_int64 },
        { "A5.4", "write_read_int32",      test_write_read_int32 },
        { "A5.5", "write_read_random_len", test_write_read_random_len },
        { "A5.6", "write_read_multi",      test_write_read_multi },
    };
    int ncases = sizeof(cases) / sizeof(cases[0]);

    return run_pack("A5", cases, ncases, filter);
}
