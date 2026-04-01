# Changelog

## Unreleased

### Frontend: tpbcli

- **Breaking:** `tpbcli list` / `l` removed. Use `tpbcli kernel list`, `kernel ls`, `k list`, or `k ls`.
- `kernel` / `k` refreshes kernel records in the workspace, then runs the subcommand (`list` / `ls` today).
- List output: columns Kernel, KernelID, Description; KernelID is six hex chars + `*`, or `[ERROR]` / `-` when applicable; failures get a tagged line via `tpb_printf` on stdout.
- New sources: `tpbcli-kernel.c`, `tpbcli-kernel-list.c` (replaces `tpbcli-list.c`).

### Corelib

- `tpb_mpik_corelib_init(void *mpi_comm, const char *)` is always declared in `tpb-public.h` (opaque communicator, e.g. `(void *)MPI_COMM_WORLD`). With MPI: coordinated init after `MPI_Init` as before. Without MPI: `tpb_corelib_mpi_stub.c` returns `TPBE_ILLEGAL_CALL`. New caller types `TPB_CORELIB_CTX_CALLER_KERNEL_MPI_MAIN_RANK` / `TPB_CORELIB_CTX_CALLER_KERNEL_MPI_SUB_RANK`. `tpb_corelib_init` / `tpb_k_corelib_init` print `TPBench is called by tpbcli|kernel|MPI kernel` after version and workspace lines. When the real MPI object is used, `libtpbench` links `MPI::MPI_C` privately.
- `tpb_register_kernel()` is part of the public API (`tpb-public.h` / `tpbench.h`).
- `tpb_list()` removed from `tpb-io`; table printing moved to the CLI.
- `tpb_k_static_info_t` adds `kernel_record_ok`; `tpb_driver_set_kernel_record_ok()` added.

#### RAFDB

Design and implement task capsule record to enclose mp/mt task records instead of unstable/dangerous merging. Task capsule record (multi-rank / multi-process grouping)

- **Public API:** `tpb_k_write_task(hdl, exit_code, task_id_out)` — when `task_id_out` is non-NULL, copy the 20-byte TaskRecordID after a successful write. `tpb_raf_gen_taskcapsule_id`; `tpb_raf_record_create_task_capsule` / `tpb_raf_record_append_task_capsule`; `tpb_k_create_capsule_task`, `tpb_k_sync_capsule_task`, `tpb_k_append_capsule_task`, `tpb_k_unlink_capsule_sync_shm`.
- **rafdb:** Capsule `.tpbr` — `dup_from` all `0xFF`, `ninput`/`noutput` 0, single header `TPBLINK::TaskID` (20-byte TaskIDs, growing 1-D); capsule ID uses SHA-1 prefix `taskcapsule` instead of `task`. Append uses advisory `fcntl` write lock; SPLIT offset derived from serialized first-header size (not stored `metasize` padding); task file `nheader` offset corrected to **172**; unbuffered stdio on append `FILE*` to avoid seek/read issues.
- **rafdb / `.tpbe`:** `entry_append_generic` uses `open(2)` + exclusive `flock(2)` + `fdopen` around read-modify-write so multiple MPI ranks can append entry rows safely; empty-file path writes start/end magic without the old `stat` + `strerror` printf.
- **Workspace:** `tpb_raf_resolve_workspace` may use `TPB_WORKSPACE` when the directory exists and corelib is not initialized (e.g. tests).
- **Autorecord:** `tpb_k_create_capsule_task` sets the workspace `.tpbe` row `dup_from` to all `0xFF` (matches capsule record layout).
- **Linking:** `tpbench` links `librt` (POSIX shm for capsule sync).
- **Tests:** `tests/corelib/test_capsule.c` (pack A6.1–A6.8), CMake target `test-capsule`.
- **Build:** `stream_mpi` kernel sources taken from `src/kernels/streaming_memory_access_mpi/` (registry name `stream_mpi` no longer assumes `simple/tpbk_stream_mpi.c`).
- **Docs:** `docs/design/design_record_EN.md` — §2.4.4 Task Capsule Record; §5 recommends capsule over merge for multi-rank grouping.

### Kernel

- During `tpb_dl_scan()`, workspace kernel record sync sets `kernel_record_ok` per loaded PLI kernel.
- AXPY: Migrate to the new PLI kernel format.
- SCALE: Migrate to the new PLI kernel format.
- TRIAD: Migrate to the new PLI kernel format.
- SUM: Migrate to the new PLI kernel format.
- RTRIAD: Migrate to the new PLI kernel format.
- STAXPY: Migrate to the new PLI kernel format.
- STRIAD: Migrate to the new PLI kernel format.
- stream_mpi: After each rank records its task (with TaskID output), `MPI_Barrier`; rank 0 `tpb_k_create_capsule_task`, `MPI_Bcast` of capsule id and status; non-root ranks append to the capsule in **ascending MPI rank order** (per-rank `MPI_Barrier` + `MPI_Allreduce` on append errors) so `TPBLINK::TaskID` order is rank 0, 1, …; rank 0 `tpb_k_unlink_capsule_sync_shm`. Removes the pre-write MPI baton ring and merge/recover-style task ID recovery (`tpb-raf-merge.c` left as-is for other callers).
- Other CPU PLI kernels: call `tpb_k_write_task(..., NULL)` for the optional TaskID argument (backward compatible).

