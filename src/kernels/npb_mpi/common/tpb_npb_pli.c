/*
 * tpb_npb_pli.c
 * Shared NPB-MPI PLI registration, entry, and MPI capsule write.
 */

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <mpi.h>

#include "tpb_npb_pli.h"
#include "tpb_npb_class.h"
#include "tpb_npb_result.h"
#include "tpb_npb_timers.h"

static int _sf_mpi_fail_if(int mpi_rc);
static int _sf_mpik_corelib_init(MPI_Comm comm,
                                 const char *tpb_workspace_path);
static int _sf_mpik_write_task(tpb_k_rthdl_t *hdl, int exit_code,
                               unsigned char *task_id_out,
                               unsigned char *tcap_id_out);
static int _sf_add_scalar_out(const char *name, const char *tag,
                              const char *note, TPB_DTYPE dtype,
                              TPB_UNIT_T unit);
static int _sf_fill_outputs(void);
static int _sf_bcast_result(MPI_Comm comm, int rank);

static int
_sf_mpi_fail_if(int mpi_rc)
{
    if (mpi_rc != MPI_SUCCESS) {
        return TPBE_MPI_FAIL;
    }
    return TPBE_SUCCESS;
}

static int
_sf_add_scalar_out(const char *name, const char *tag, const char *note,
                   TPB_DTYPE dtype, TPB_UNIT_T unit)
{
    return tpb_k_add_output(name, tag, note, dtype,
        unit | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_N | TPB_UATTR_SHAPE_POINT);
}

int
tpb_npb_pli_register(const char *kname, const char *note,
                     const char *class_list, int has_graph)
{
    int err;
    int nclass;
    int64_t dtimer[2];

    if (kname == NULL || note == NULL || class_list == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    err = tpb_k_register(kname, note, TPB_KTYPE_PLI);
    if (err != 0) {
        return err;
    }

    nclass = (int)strlen(class_list);
    err = tpb_k_add_arg("class", NULL, "NPB problem class", "S",
        TPB_PARM_CLI | TPB_CHAR_T | TPB_PARM_LIST, nclass, class_list);
    if (err != 0) {
        return err;
    }

    dtimer[0] = 0;
    dtimer[1] = 1;
    err = tpb_k_add_arg("detailed_timer", NULL,
        "NPB extra timers (dummy in first draft)", "0",
        TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_LIST, 2, dtimer);
    if (err != 0) {
        return err;
    }

    if (has_graph) {
        err = tpb_k_add_arg("graph", NULL,
            "DT graph: B=BH, W=WH, S=SH", "B",
            TPB_PARM_CLI | TPB_CHAR_T | TPB_PARM_LIST, 3, "BWS");
        if (err != 0) {
            return err;
        }
    }

    err = _sf_add_scalar_out("Time", TPB_TAG_FOM,
        "Official NPB Time in seconds (MPI_MAX of the total-timer slot).",
        TPB_DOUBLE_T, TPB_UNIT_SS);
    if (err != 0) {
        return err;
    }
    err = _sf_add_scalar_out("Mop/s", TPB_TAG_FOM,
        "Official NPB Mop/s (DT: bytes transmitted rate).",
        TPB_DOUBLE_T, TPB_UNIT_OPS);
    if (err != 0) {
        return err;
    }
    err = _sf_add_scalar_out("verified", TPB_TAG_VERIFYVAR,
        "Official NPB verification flag (1=SUCCESSFUL).",
        TPB_INT32_T, TPB_UNAME_UNDEF | TPB_UBASE_BASE);
    if (err != 0) {
        return err;
    }
    err = _sf_add_scalar_out("niter", TPB_TAG_INPUT,
        "Official NPB iteration count.",
        TPB_INT32_T, TPB_UNAME_UNDEF | TPB_UBASE_BASE);
    if (err != 0) {
        return err;
    }
    err = _sf_add_scalar_out("n1", TPB_TAG_INPUT,
        "Official NPB size n1 (or problem size).",
        TPB_INT32_T, TPB_UNAME_UNDEF | TPB_UBASE_BASE);
    if (err != 0) {
        return err;
    }
    err = _sf_add_scalar_out("n2", TPB_TAG_INPUT,
        "Official NPB size n2 (0 if unused).",
        TPB_INT32_T, TPB_UNAME_UNDEF | TPB_UBASE_BASE);
    if (err != 0) {
        return err;
    }
    err = _sf_add_scalar_out("n3", TPB_TAG_INPUT,
        "Official NPB size n3 (0 if unused).",
        TPB_INT32_T, TPB_UNAME_UNDEF | TPB_UBASE_BASE);
    if (err != 0) {
        return err;
    }
    err = _sf_add_scalar_out("npb_class", TPB_TAG_INPUT,
        "NPB problem class letter recorded from the official report.",
        TPB_CHAR_T, TPB_UNAME_UNDEF | TPB_UBASE_BASE);
    if (err != 0) {
        return err;
    }

    return tpb_k_finalize_pli();
}

static int
_sf_fill_outputs(void)
{
    int err;
    double *ptime;
    double *pmops;
    int32_t *pver;
    int32_t *pniter;
    int32_t *pn1;
    int32_t *pn2;
    int32_t *pn3;
    char *pclass;

    ptime = NULL;
    pmops = NULL;
    pver = NULL;
    pniter = NULL;
    pn1 = NULL;
    pn2 = NULL;
    pn3 = NULL;
    pclass = NULL;

    err = tpb_k_alloc_output("Time", 1, &ptime);
    if (err != 0 || ptime == NULL) {
        return (err != 0) ? err : TPBE_MALLOC_FAIL;
    }
    err = tpb_k_alloc_output("Mop/s", 1, &pmops);
    if (err != 0 || pmops == NULL) {
        return (err != 0) ? err : TPBE_MALLOC_FAIL;
    }
    err = tpb_k_alloc_output("verified", 1, &pver);
    if (err != 0 || pver == NULL) {
        return (err != 0) ? err : TPBE_MALLOC_FAIL;
    }
    err = tpb_k_alloc_output("niter", 1, &pniter);
    if (err != 0 || pniter == NULL) {
        return (err != 0) ? err : TPBE_MALLOC_FAIL;
    }
    err = tpb_k_alloc_output("n1", 1, &pn1);
    if (err != 0 || pn1 == NULL) {
        return (err != 0) ? err : TPBE_MALLOC_FAIL;
    }
    err = tpb_k_alloc_output("n2", 1, &pn2);
    if (err != 0 || pn2 == NULL) {
        return (err != 0) ? err : TPBE_MALLOC_FAIL;
    }
    err = tpb_k_alloc_output("n3", 1, &pn3);
    if (err != 0 || pn3 == NULL) {
        return (err != 0) ? err : TPBE_MALLOC_FAIL;
    }
    err = tpb_k_alloc_output("npb_class", 1, &pclass);
    if (err != 0 || pclass == NULL) {
        return (err != 0) ? err : TPBE_MALLOC_FAIL;
    }

    *ptime = tpb_npb_result_time();
    *pmops = tpb_npb_result_mops();
    *pver = tpb_npb_result_verified();
    *pniter = tpb_npb_result_niter();
    *pn1 = tpb_npb_result_n1();
    *pn2 = tpb_npb_result_n2();
    *pn3 = tpb_npb_result_n3();
    *pclass = tpb_npb_result_class();
    return TPBE_SUCCESS;
}

static int
_sf_bcast_result(MPI_Comm comm, int rank)
{
    double time_sec;
    double mops;
    int verified;
    int niter;
    int n1;
    int n2;
    int n3;
    char cls;
    int merr;

    time_sec = tpb_npb_result_time();
    mops = tpb_npb_result_mops();
    verified = tpb_npb_result_verified();
    niter = tpb_npb_result_niter();
    n1 = tpb_npb_result_n1();
    n2 = tpb_npb_result_n2();
    n3 = tpb_npb_result_n3();
    cls = tpb_npb_result_class();

    merr = _sf_mpi_fail_if(MPI_Bcast(&time_sec, 1, MPI_DOUBLE, 0, comm));
    if (merr != TPBE_SUCCESS) {
        return merr;
    }
    merr = _sf_mpi_fail_if(MPI_Bcast(&mops, 1, MPI_DOUBLE, 0, comm));
    if (merr != TPBE_SUCCESS) {
        return merr;
    }
    merr = _sf_mpi_fail_if(MPI_Bcast(&verified, 1, MPI_INT, 0, comm));
    if (merr != TPBE_SUCCESS) {
        return merr;
    }
    merr = _sf_mpi_fail_if(MPI_Bcast(&niter, 1, MPI_INT, 0, comm));
    if (merr != TPBE_SUCCESS) {
        return merr;
    }
    merr = _sf_mpi_fail_if(MPI_Bcast(&n1, 1, MPI_INT, 0, comm));
    if (merr != TPBE_SUCCESS) {
        return merr;
    }
    merr = _sf_mpi_fail_if(MPI_Bcast(&n2, 1, MPI_INT, 0, comm));
    if (merr != TPBE_SUCCESS) {
        return merr;
    }
    merr = _sf_mpi_fail_if(MPI_Bcast(&n3, 1, MPI_INT, 0, comm));
    if (merr != TPBE_SUCCESS) {
        return merr;
    }
    merr = _sf_mpi_fail_if(MPI_Bcast(&cls, 1, MPI_CHAR, 0, comm));
    if (merr != TPBE_SUCCESS) {
        return merr;
    }

    if (rank != 0) {
        tpb_npb_set_result(time_sec, mops, verified, niter, n1, n2, n3,
            cls);
    }
    return TPBE_SUCCESS;
}

int
tpb_npb_pli_entry(int argc, char **argv, const char *kname,
                  const char *bench, int (*reg_fn)(void),
                  tpb_npb_run_fn run_fn, int has_graph)
{
    int err;
    int rank;
    int nprocs;
    const char *timer_name;
    unsigned char my_task_id[20];
    unsigned char capsule_id[20];
    unsigned char kernel_id_bin[20];
    uint32_t handle_index;
    int cap_err;
    const char *ev;
    char cls;
    char graph;
    int64_t detailed;
    int run_rc;
    int rec_err;

    if (kname == NULL || bench == NULL || reg_fn == NULL ||
        run_fn == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    memset(my_task_id, 0, sizeof(my_task_id));
    memset(capsule_id, 0, sizeof(capsule_id));
    memset(kernel_id_bin, 0, sizeof(kernel_id_bin));
    handle_index = 0;
    graph = 'B';
    detailed = 0;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);

    err = _sf_mpik_corelib_init(MPI_COMM_WORLD, NULL);
    if (err != 0) {
        if (rank == 0) {
            tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO,
                TPBLOG_FLAG_DIRECT,
                "Error: %s corelib init failed: %d\n", kname, err);
        }
        MPI_Finalize();
        return err;
    }

    if (argc >= 2) {
        timer_name = argv[1];
    } else {
        timer_name = getenv("TPBENCH_TIMER");
    }
    if (timer_name == NULL) {
        if (rank == 0) {
            tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO,
                TPBLOG_FLAG_DIRECT,
                "Error: Timer not specified (argv[1] or TPBENCH_TIMER)\n");
        }
        MPI_Finalize();
        return TPBE_CLI_FAIL;
    }

    err = tpb_k_pli_set_timer(timer_name);
    if (err != 0) {
        if (rank == 0) {
            tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO,
                TPBLOG_FLAG_DIRECT,
                "Error: Failed to set timer '%s'\n", timer_name);
        }
        MPI_Finalize();
        return err;
    }

    err = reg_fn();
    if (err != 0) {
        if (rank == 0) {
            tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO,
                TPBLOG_FLAG_DIRECT,
                "Error: Failed to register kernel %s\n", kname);
        }
        MPI_Finalize();
        return err;
    }

    {
        tpb_k_rthdl_t handle;

        err = tpb_k_pli_build_handle(&handle, argc - 2, argv + 2);
        if (err != 0) {
            if (rank == 0) {
                tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO,
                    TPBLOG_FLAG_DIRECT,
                    "Error: Failed to build handle\n");
            }
            MPI_Finalize();
            return err;
        }

        if (rank == 0) {
            tpb_cliout_args(&handle);
            tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                TPBLOG_FLAG_DIRECT, "Kernel logs\n");
        }

        ev = getenv("TPB_HANDLE_INDEX");
        if (ev != NULL) {
            handle_index = (uint32_t)atoi(ev);
        }
        ev = getenv("TPB_KERNEL_ID");
        if (ev != NULL && strlen(ev) == 40) {
            (void)tpb_raf_hex_to_id(ev, kernel_id_bin);
        }

        err = tpb_npb_timers_bind();
        if (err != 0) {
            if (rank == 0) {
                tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO,
                    TPBLOG_FLAG_DIRECT,
                    "Error: NPB timer bind failed: %d\n", err);
            }
            tpb_driver_clean_handle(&handle);
            MPI_Finalize();
            return err;
        }

        err = tpb_k_get_arg_char("class", &cls);
        if (err != 0) {
            tpb_driver_clean_handle(&handle);
            MPI_Finalize();
            return err;
        }
        (void)tpb_k_get_arg_i64("detailed_timer", &detailed);
        (void)detailed;
        if (has_graph) {
            err = tpb_k_get_arg_char("graph", &graph);
            if (err != 0) {
                tpb_driver_clean_handle(&handle);
                MPI_Finalize();
                return err;
            }
        }

        tpb_npb_result_clear();
        err = tpb_npb_apply_class(bench, cls);
        if (err != 0) {
            if (rank == 0) {
                tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO,
                    TPBLOG_FLAG_DIRECT,
                    "Error: illegal NPB class '%c' for %s\n", cls,
                    bench);
            }
            tpb_driver_clean_handle(&handle);
            MPI_Finalize();
            return err;
        }

        err = tpb_npb_check_nprocs(bench, nprocs, graph);
        if (err != 0) {
            if (rank == 0) {
                tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO,
                    TPBLOG_FLAG_DIRECT,
                    "Error: illegal nprocs=%d for %s class %c\n",
                    nprocs, bench, cls);
            }
            tpb_driver_clean_handle(&handle);
            MPI_Finalize();
            return err;
        }

        run_rc = run_fn(graph);
        if (run_rc != TPBE_SUCCESS) {
            rec_err = (run_rc == 1) ? TPBE_KERN_ARG_FAIL : run_rc;
            if (rank == 0) {
                tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO,
                    TPBLOG_FLAG_TSTAG,
                    "Rank %d: Kernel %s failed: %d\n", rank, kname,
                    rec_err);
            }
            (void)tpb_k_write_task(&handle, rec_err, NULL);
            tpb_driver_clean_handle(&handle);
            MPI_Finalize();
            return rec_err;
        }

        err = _sf_bcast_result(MPI_COMM_WORLD, rank);
        if (err != 0) {
            tpb_driver_clean_handle(&handle);
            MPI_Finalize();
            return err;
        }

        err = _sf_fill_outputs();
        if (err != 0) {
            tpb_driver_clean_handle(&handle);
            MPI_Finalize();
            return err;
        }

        rec_err = TPBE_SUCCESS;
        if (tpb_npb_result_verified() == 0) {
            rec_err = TPBE_KERN_VERIFY_FAIL;
        }

        cap_err = _sf_mpik_write_task(&handle, rec_err, my_task_id,
            capsule_id);
        if (rank == 0) {
            if (cap_err == 0) {
                tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                    TPBLOG_FLAG_DIRECT,
                    "Rank 0 created task capsule ended.\n");
            } else {
                tpblog_printf_f(TPB_LOG_LEVEL_WARN, TPBLOG_TYPE_WARN,
                    TPBLOG_FLAG_TSTAG,
                    "%s: task capsule write failed %d\n", kname,
                    cap_err);
            }
        }

        MPI_Barrier(MPI_COMM_WORLD);
        if (rank == 0 && cap_err == 0) {
            (void)tpb_k_unlink_capsule_sync_shm(kernel_id_bin,
                handle_index);
        }

        tpb_driver_clean_handle(&handle);
        MPI_Finalize();
        if (cap_err != TPBE_SUCCESS) {
            return cap_err;
        }
        return rec_err;
    }
}

static int
_sf_mpik_corelib_init(MPI_Comm comm, const char *tpb_workspace_path)
{
    int rank;
    int nprocs;
    int bcast_err;
    int local_err;
    int max_err;
    int local_pid;
    int *allpids;
    int gather_alloc_fail;
    int max_alloc_fail;
    int r;
    int merr;

    bcast_err = 0;
    merr = _sf_mpi_fail_if(MPI_Comm_rank(comm, &rank));
    if (merr != TPBE_SUCCESS) {
        return merr;
    }
    merr = _sf_mpi_fail_if(MPI_Comm_size(comm, &nprocs));
    if (merr != TPBE_SUCCESS) {
        return merr;
    }

    if (rank == 0) {
        bcast_err = tpb_k_corelib_init(tpb_workspace_path);
    }

    merr = _sf_mpi_fail_if(MPI_Bcast(&bcast_err, 1, MPI_INT, 0, comm));
    if (merr != TPBE_SUCCESS) {
        return merr;
    }
    if (bcast_err != TPBE_SUCCESS) {
        return bcast_err;
    }

    if (rank == 0) {
        local_err = TPBE_SUCCESS;
    } else {
        local_err = tpb_k_corelib_init(tpb_workspace_path);
    }

    merr = _sf_mpi_fail_if(MPI_Allreduce(&local_err, &max_err, 1,
        MPI_INT, MPI_MAX, comm));
    if (merr != TPBE_SUCCESS) {
        return merr;
    }
    if (max_err != TPBE_SUCCESS) {
        return max_err;
    }

    allpids = NULL;
    if (rank == 0) {
        allpids = calloc((size_t)nprocs, sizeof *allpids);
    }
    gather_alloc_fail = (rank == 0 && allpids == NULL) ? 1 : 0;
    merr = _sf_mpi_fail_if(MPI_Allreduce(&gather_alloc_fail,
        &max_alloc_fail, 1, MPI_INT, MPI_MAX, comm));
    if (merr != TPBE_SUCCESS) {
        free(allpids);
        return merr;
    }
    if (max_alloc_fail != 0) {
        return TPBE_MALLOC_FAIL;
    }

    local_pid = (int)getpid();
    merr = _sf_mpi_fail_if(MPI_Gather(&local_pid, 1, MPI_INT, allpids, 1,
        MPI_INT, 0, comm));
    if (merr != TPBE_SUCCESS) {
        free(allpids);
        return merr;
    }

    if (rank == 0) {
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
            TPBLOG_FLAG_DIRECT,
            "TPBench is now initialized by rank: ");
        for (r = 0; r < nprocs; r++) {
            tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                TPBLOG_FLAG_DIRECT, "%d (pid=%d)%s",
                r, allpids[r], (r + 1 < nprocs) ? ", " : "");
        }
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
            TPBLOG_FLAG_DIRECT, "\n");
        free(allpids);
    }

    merr = _sf_mpi_fail_if(MPI_Barrier(comm));
    if (merr != TPBE_SUCCESS) {
        return merr;
    }

    if (rank == 0) {
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
            TPBLOG_FLAG_DIRECT,
            "TPBench has been initialized by the MPI kernel.\n");
    }

    return TPBE_SUCCESS;
}

static int
_sf_mpik_write_task(tpb_k_rthdl_t *hdl, int exit_code,
                    unsigned char *task_id_out,
                    unsigned char *tcap_id_out)
{
    MPI_Comm comm = MPI_COMM_WORLD;
    int rank;
    int nprocs;
    unsigned char my_task_id[20];
    unsigned char capsule_id[20];
    int werr;
    int cap_err;
    int merr;
    int derive_err;
    int max_derive_err;
    unsigned char *all_ids;
    int rank0_alloc_ok;
    int min_alloc_ok;
    int ar;
    int step_err;
    int append_agg;
    int final_err;

    if (hdl == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    merr = _sf_mpi_fail_if(MPI_Comm_rank(comm, &rank));
    if (merr != TPBE_SUCCESS) {
        return merr;
    }
    merr = _sf_mpi_fail_if(MPI_Comm_size(comm, &nprocs));
    if (merr != TPBE_SUCCESS) {
        return merr;
    }

    memset(my_task_id, 0, sizeof(my_task_id));
    memset(capsule_id, 0, sizeof(capsule_id));

    werr = tpb_k_write_task(hdl, exit_code, my_task_id);

    merr = _sf_mpi_fail_if(MPI_Barrier(comm));
    if (merr != TPBE_SUCCESS) {
        return merr;
    }

    cap_err = TPBE_SUCCESS;
    if (rank == 0) {
        if (werr == TPBE_SUCCESS) {
            cap_err = tpb_k_create_capsule_task(my_task_id, capsule_id);
        } else {
            cap_err = werr;
        }
    }

    merr = _sf_mpi_fail_if(MPI_Bcast(capsule_id, 20, MPI_BYTE, 0, comm));
    if (merr != TPBE_SUCCESS) {
        return merr;
    }
    merr = _sf_mpi_fail_if(MPI_Bcast(&cap_err, 1, MPI_INT, 0, comm));
    if (merr != TPBE_SUCCESS) {
        return merr;
    }

    final_err = cap_err;

    if (cap_err == TPBE_SUCCESS) {
        derive_err = tpb_k_task_set_derive_to(my_task_id, capsule_id);
        merr = _sf_mpi_fail_if(MPI_Allreduce(&derive_err, &max_derive_err,
            1, MPI_INT, MPI_MAX, comm));
        if (merr != TPBE_SUCCESS) {
            return merr;
        }
        if (max_derive_err != TPBE_SUCCESS) {
            final_err = max_derive_err;
        }
    }

    if (cap_err == TPBE_SUCCESS && nprocs >= 2) {
        all_ids = NULL;
        rank0_alloc_ok = 1;
        if (rank == 0) {
            all_ids = calloc((size_t)nprocs, (size_t)20);
            if (all_ids == NULL) {
                rank0_alloc_ok = 0;
            }
        }
        merr = _sf_mpi_fail_if(MPI_Allreduce(&rank0_alloc_ok,
            &min_alloc_ok, 1, MPI_INT, MPI_MIN, comm));
        if (merr != TPBE_SUCCESS) {
            free(all_ids);
            return merr;
        }
        if (min_alloc_ok == 0) {
            free(all_ids);
            if (task_id_out != NULL) {
                memcpy(task_id_out, my_task_id, (size_t)20);
            }
            if (tcap_id_out != NULL) {
                memcpy(tcap_id_out, capsule_id, (size_t)20);
            }
            return TPBE_MALLOC_FAIL;
        }

        merr = _sf_mpi_fail_if(MPI_Gather(my_task_id, 20, MPI_BYTE,
            all_ids, 20, MPI_BYTE, 0, comm));
        if (merr != TPBE_SUCCESS) {
            free(all_ids);
            return merr;
        }

        append_agg = TPBE_SUCCESS;
        if (rank == 0) {
            for (ar = 1; ar < nprocs; ar++) {
                step_err = tpb_k_append_capsule_task(capsule_id,
                    all_ids + (size_t)ar * (size_t)20);
                if (step_err != TPBE_SUCCESS) {
                    append_agg = step_err;
                    break;
                }
            }
            free(all_ids);
        }
        merr = _sf_mpi_fail_if(MPI_Bcast(&append_agg, 1, MPI_INT, 0,
            comm));
        if (merr != TPBE_SUCCESS) {
            return merr;
        }
        if (append_agg != TPBE_SUCCESS) {
            final_err = append_agg;
        }
    }

    if (task_id_out != NULL) {
        memcpy(task_id_out, my_task_id, (size_t)20);
    }
    if (tcap_id_out != NULL) {
        memcpy(tcap_id_out, capsule_id, (size_t)20);
    }

    if (final_err == TPBE_SUCCESS && rank == 0) {
        char cap_hex[41];

        tpb_raf_id_to_hex(capsule_id, cap_hex);
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
            TPBLOG_FLAG_TSTAG,
            "MPI Task recorded. Task capsule ID = %s, included %d "
            "tasks\n",
            cap_hex, nprocs);
    }

    return final_err;
}
