# NPB_TIMER_FLAG and PERRANK / AGGREGATE records

NPB section timers are compiled in with **`NPB_TIMER_FLAG`**. Configure:

```bash
cmake -B build-npb-t1 -DTPB_KERNELS=bt_mpi,sp_mpi,lu_mpi,cg_mpi,mg_mpi,ft_mpi,ep_mpi,is_mpi,dt_mpi \
  -DTPB_NPB_TIMER_FLAG=1
# or
tpbcli kernel build --kernel-tag npb --npb-timer-flag 1
```

`check_timer_flag()` in `tpb_npb_timers.c` returns that macro. The Fortran wrapper calls the C function, so `.f90` sources do not need `-cpp`.

## Record layout

Each MPI rank writes its own `.tpbr`. Rank 0 also writes a capsule that lists member TaskIDs. `subrank` is capsule insertion order and is not the MPI rank. Use the recorded **`mpi_rank`** / **`cpu_id`** columns as join keys.

| Outputs | Who allocates | Tag | Meaning |
| -------- | ------------- | --- | ------- |
| `mpi_rank`, `mpi_nprocs`, `cpu_id` | every rank | PERRANK | Identity |
| `Time_local` | every rank | PERRANK | This rank's official total-timer slot |
| `npb_timer_flag` | every rank | TPBINPUT | Compiled flag |
| `Time`, `Mop/s`, `verified`, `niter`, `n1`, `n2`, `n3`, `npb_class` | rank 0 only | AGGREGATE (+ FOM / VERIFYVAR) | Official NPB report |
| `<name>` (1-D) | every rank, flag=1 | PERRANK | Per-call intervals for that timing point |
| `<name>_total`, `<name>_derived`, `<name>_ncall` | every rank, flag=1 | PERRANK | Cumulative, post-derivation, call count |
| `<name>_min`, `<name>_max`, `<name>_avg` | rank 0, flag=1 | AGGREGATE | NPB SECTION table |

Each output is one contiguous array in the `.tpbr` payload. Do not interleave ranks or variables.

## Heatmap

```bash
tpbcli task export -r 0 --outdir ./exp --keep-current
python3 scripts/npb_timer_matrix.py ./exp --timer t_blts --reshape-lu 300 408
```

The script writes `npb_timer_tidy.csv` and a rank-by-call-index heatmap. For LU, `--reshape-lu ITMAX NZ` treats the call index as iteration-major over k-planes (`nz-2` planes).
