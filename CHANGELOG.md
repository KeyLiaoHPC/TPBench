# Changelog

## Unreleased

### Frontend: tpbcli

- **Breaking:** `tpbcli list` / `l` removed. Use `tpbcli kernel list`, `kernel ls`, `k list`, or `k ls`.
- `kernel` / `k` refreshes kernel records in the workspace, then runs the subcommand (`list` / `ls` today).
- List output: columns Kernel, KernelID, Description; KernelID is six hex chars + `*`, or `[ERROR]` / `-` when applicable; failures get a tagged line via `tpb_printf` on stdout.
- New sources: `tpbcli-kernel.c`, `tpbcli-kernel-list.c` (replaces `tpbcli-list.c`).

### Corelib

- `tpb_register_kernel()` is part of the public API (`tpb-public.h` / `tpbench.h`).
- `tpb_list()` removed from `tpb-io`; table printing moved to the CLI.
- `tpb_k_static_info_t` adds `kernel_record_ok`; `tpb_driver_set_kernel_record_ok()` added.

### Kernel

- During `tpb_dl_scan()`, workspace kernel record sync sets `kernel_record_ok` per loaded PLI kernel.
- AXPY: Migrate to the new PLI kernel format.
- SCALE: Migrate to the new PLI kernel format.
- TRIAD: Migrate to the new PLI kernel format.
- SUM: Migrate to the new PLI kernel format.
- RTRIAD: Migrate to the new PLI kernel format.
- STAXPY: Migrate to the new PLI kernel format.
- STRIAD: Migrate to the new PLI kernel format.