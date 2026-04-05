# Changelog

## Unreleased

### Frontend: tpbcli

- **Breaking:** `tpbcli run` no longer accepts `--kargs`, `--kargs-dim`, `--kenvs`, `--kenvs-dim`, `--kmpiargs`, or `--kmpiargs-dim` before a preceding `--kernel`. Each such option must follow the `--kernel` it applies to. The `_tpb_common` pseudo handle (`handle_list[0]`) and default-parameter inheritance from it are removed. Multiple `--kmpiargs` after the same `--kernel` concatenate with a space.
- **Tests:** Pack B2 (`tests/tpbcli/test-cli-run-nokernel.c`) — expect CLI failure with “preceding --kernel” when k-options precede `--kernel`; B2.5 sanity run with `--kernel stream` first.
- **Breaking:** `database dump --entry task` prints only task `.tpbe` rows with `derive_to` all-zero (task entry points); line `(N task entry points / M total rows)` reports filtered vs total row count.
- **Breaking:** `tpbcli list` / `l` removed. Use `tpbcli kernel list`, `kernel ls`, `k list`, or `k ls`.
- `kernel` / `k` refreshes kernel records in the workspace, then runs the subcommand (`list` / `ls` today).
- List output: columns Kernel, KernelID, Description; KernelID is six hex chars + `*`, or `[ERROR]` / `-` when applicable; failures get a tagged line via `tpb_printf` on stdout.
- New sources: `tpbcli-kernel.c`, `tpbcli-kernel-list.c` (replaces `tpbcli-list.c`).

### Corelib

- **Breaking:** TBatch `.tpbr` auto-record layout: two headers `TPBLINK::TaskID` (append 20-byte TaskRecordIDs) and `TPBLINK::KernelID` (empty data for now), replacing `KernelRecordIDs` / `TaskRecordIDs` / `ScoreRecordIDs`. Skeleton `.tpbr` is written at `tpb_record_begin_batch`; `tpb_record_end_batch` runs an internal scan of `task.tpbe` (rows with `utc_bits` not before batch start, matching `tbatch_id`, `derive_to` all-zero) and appends each via `tpb_raf_record_append_tbatch`, then patches `duration` / `ntask` / `nkernel` with `tpb_raf_record_patch_tbatch_counters`.
- **Breaking:** RAFDB lineage fields renamed: `dup_from` → `inherit_from`, `dup_to` → `derive_to` in `tbatch_attr_t` / `tbatch_entry_t` / `kernel_attr_t` / `kernel_entry_t` / `task_attr_t` / `task_entry_t` and in CLI dump key names. `tpb_k_task_set_derive_to(task_id, derive_to_id)` replaces `tpb_k_task_set_dup_to`. On-disk layout byte positions unchanged; existing workspaces are not read as compatible.
- Auto-record tbatch finalization sets `ntask` and fills `TPBLINK::TaskID` from task `.tpbe` rows that match the batch (including `utc_bits` not before batch start), with `derive_to` all-zero (one logical task per MPI invocation when a capsule is used). Unique-kernel counting (`nkernel`) uses the same entry-point filter so the capsule row is not skipped as a duplicate of per-rank rows.
- `tpb_mpik_corelib_init(void *mpi_comm, const char *)` is always declared in `tpb-public.h` (opaque communicator, e.g. `(void *)MPI_COMM_WORLD`). With MPI: coordinated init after `MPI_Init` as before; stores the communicator for `tpb_mpik_write_task`. Without MPI: `tpb_corelib_mpi_stub.c` returns `TPBE_ILLEGAL_CALL`. New caller types `TPB_CORELIB_CTX_CALLER_KERNEL_MPI_MAIN_RANK` / `TPB_CORELIB_CTX_CALLER_KERNEL_MPI_SUB_RANK`. `tpb_corelib_init` / `tpb_k_corelib_init` print `TPBench is called by tpbcli|kernel|MPI kernel` after version and workspace lines. When the real MPI object is used, `libtpbench` links `MPI::MPI_C` privately.
- `tpb_mpik_write_task(hdl, exit_code, task_id_out, tcap_id_out)` — MPI-coordinated `tpb_k_write_task` + capsule: rank 0 creates capsule and `MPI_Bcast`s ID; each rank patches `derive_to` via `tpb_k_task_set_derive_to`; `MPI_Gather` + rank-0-only `tpb_k_append_capsule_task` for ranks 1..n-1. Stub returns `TPBE_ILLEGAL_CALL` without MPI.
- `tpb_k_task_set_derive_to(task_id, derive_to_id)` — seek-and-patch `derive_to` in task `.tpbr` (validated magic + task ID) and matching `task.tpbe` row (`flock` on entry file).
- `tpb_register_kernel()` is part of the public API (`tpb-public.h` / `tpbench.h`).
- `tpb_list()` removed from `tpb-io`; table printing moved to the CLI.
- `tpb_k_static_info_t` adds `kernel_record_ok`; `tpb_driver_set_kernel_record_ok()` added.
- **At 20260402**: Refactor APIs according to the update `STYLE_GUIDE.md` 

#### RAFDB

- **Public API:** `tpb_raf_record_append_tbatch`, `tpb_raf_record_patch_tbatch_counters` — append TaskRecordID to tbatch `.tpbr` under lock; patch duration and counts after scan.

Design and implement task capsule record to enclose mp/mt task records instead of unstable/dangerous merging. Task capsule record (multi-rank / multi-process grouping)

- **Public API:** `tpb_k_write_task(hdl, exit_code, task_id_out)` — when `task_id_out` is non-NULL, copy the 20-byte TaskRecordID after a successful write. `tpb_raf_gen_taskcapsule_id`; `tpb_raf_record_create_task_capsule` / `tpb_raf_record_append_task_capsule`; `tpb_k_create_capsule_task`, `tpb_k_sync_capsule_task`, `tpb_k_append_capsule_task`, `tpb_k_unlink_capsule_sync_shm`.
- **rafdb:** Capsule `.tpbr` — `inherit_from` all `0xFF`, `ninput`/`noutput` 0, single header `TPBLINK::TaskID` (20-byte TaskIDs, growing 1-D); capsule ID uses SHA-1 prefix `taskcapsule` instead of `task`. Append uses advisory `fcntl` write lock; SPLIT offset derived from serialized first-header size (not stored `metasize` padding); task file `nheader` offset corrected to **172**; unbuffered stdio on append `FILE*` to avoid seek/read issues.
- **rafdb / `.tpbe`:** `entry_append_generic` uses `open(2)` + exclusive `flock(2)` + `fdopen` around read-modify-write so multiple MPI ranks can append entry rows safely; empty-file path writes start/end magic without the old `stat` + `strerror` printf.
- **Workspace:** `tpb_raf_resolve_workspace` may use `TPB_WORKSPACE` when the directory exists and corelib is not initialized (e.g. tests).
- **Autorecord:** `tpb_k_create_capsule_task` sets the workspace `.tpbe` row `inherit_from` to all `0xFF` (matches capsule record layout).
- **Autorecord / task `.tpbr`:** `tpb_record_write_task` writes output headers and payload only for outputs that were allocated (`out->n > 0` and `out->p != NULL`). Skipped slots remain registered via `tpb_k_add_output` for CLI/schema but are omitted from the on-disk task record; `attr.noutput` and `attr.nheader` reflect the serialized subset. **Tests:** `tests/corelib/test_1d_array_write.c` pack A5.7 (`skip_unalloc_output`).
- **Linking:** `tpbench` links `librt` (POSIX shm for capsule sync).
- **Tests:** `tests/corelib/test_capsule.c` (pack A6.1–A6.8), CMake target `test-capsule`.
- **Build:** `stream_mpi` kernel sources taken from `src/kernels/streaming_memory_access_mpi/` (registry name `stream_mpi` no longer assumes `simple/tpbk_stream_mpi.c`).
- **Docs:** `docs/design/design_record_EN.md` / `design_record_CN.md` — `inherit_from` / `derive_to`, task entry point semantics, §2.4.4 Task Capsule Record; §5 recommends capsule over merge for multi-rank grouping. `docs/API_Reference.md` struct excerpts updated.

### Kernel

- During `tpb_dl_scan()`, workspace kernel record sync sets `kernel_record_ok` per loaded PLI kernel.
- AXPY: Migrate to the new PLI kernel format.
- SCALE: Migrate to the new PLI kernel format.
- TRIAD: Migrate to the new PLI kernel format.
- SUM: Migrate to the new PLI kernel format.
- RTRIAD: Migrate to the new PLI kernel format.
- STAXPY: Migrate to the new PLI kernel format.
- STRIAD: Migrate to the new PLI kernel format.
- stream_mpi: Success path calls `tpb_mpik_write_task` (corelib MPI collectives + `derive_to` patch + rank-0 capsule appends); rank 0 `tpb_k_unlink_capsule_sync_shm` after barrier. Error path still uses `tpb_k_write_task` only.
- stream_mpi: Static PLI registration (`tpb_k_pli_register_stream_mpi`) registers all outputs on every rank (kernel `.so` scan / handle build): `INPARM::*`, per-iteration `EVENT,TIME::Copy/Scale/Add/Triad`, and sixteen aggregate `FOM,BANDWIDTH::…` / `FOM,TIME::…` summaries. Only MPI rank 0 calls `tpb_k_alloc_output` for the FOM slots; other ranks leave them unallocated so task `.tpbr` records omit FOM payload (corelib skips unallocated outputs).
- Other CPU PLI kernels: call `tpb_k_write_task(..., NULL)` for the optional TaskID argument (backward compatible).

