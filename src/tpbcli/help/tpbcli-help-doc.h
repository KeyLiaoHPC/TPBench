/**
 * @file tpbcli-help-doc.h
 * @brief Top-level tpbcli help text.
 */

#ifndef TPBCLI_HELP_DOC_H
#define TPBCLI_HELP_DOC_H

#define TPBCLI_HELP_DOC_TOTAL \
    "tpbcli is the command-line interface of the active launcher of TPBench.\n" \
    "Usage: tpbcli [--workspace PATH] <action> <option>\n" \
    "Global (before action): --workspace PATH — TPBench data root; omit to use $TPB_WORKSPACE or $HOME/.tpbench.\n" \
    "Action: run/r, benchmark/b, database/d, kernel/k, help/h\n" \
    "Options and explanation for each action:\n" \
    "    r, run: Run one or more benchmark kernels.\n" \
    "        -P          Use Process-Level Integration mode (default). Kernels run as\n" \
    "                    separate processes via fork/exec.\n" \
    "        -F          Use Function-Level Integration mode. Kernels run as linked\n" \
    "                    functions within the same process.\n" \
    "        --kernel    <kernel_name>\n" \
    "                    Mandatory. Kernel list separated by comma. Each kernel can have\n" \
    "                    multiple kargs separated by colon. (e.g.\n" \
    "                    d_init:total_memsize=1024,triad:fp=fp64:total_memsize=1024)\n" \
    "                    Use 'help <kernel>' for available options of a kernel.\n" \
    "        --kargs     <key>=<value>\n" \
    "                    Optional. Default kernel run-time arguments.\n" \
    "                    Use 'help kargs' for supported keys and values.\n" \
    "        --kargs_dim \n" \
    "                    Optional. Create one or more dimensional arguments, and TPBench will \n" \
    "                    execute the kernel with arguments along the designated dimension. If \n" \
    "                    " \
    "        --workdir   <PATH> (Default: $CWD/workspace)\n" \
    "                    Optional. Path to the directory of the workspace. \n" \
    "        --outdir    <PATH> (Default: $CWD/workspace/<tpbrun-YYYYMMDDThhmmss>)\n" \
    "                    Optional.Path to the data directory of the current test. \n" \
    "        --timer     <timer_name> (Optional, default: clock_gettime)\n" \
    "                    Optional. Timer name. Supported: clock_gettime, tsc_asym.\n" \
    "        -l          List available kernels.\n" \
    "        -h, --help  Print usages of the `tpbcli run` subcommand.\n" \
    "    b, benchmark: Run predefined benchmark suites.\n" \
    "        --suite     <PATH> (Default: $CWD/workspace)\n" \
    "        --workdir   <PATH> (Optional. Default: $CWD/workspace)\n" \
    "                    Path to the directory of the workspace. \n" \
    "        --outdir    <PATH> (Optional. Default: $CWD/workspace/<tpbrun-YYYYMMDDThhmmss>)\n" \
    "                    Path to the data directory of the current test. \n" \
    "        -h, --help  Print usages of the `tpbcli benchmark` subcommand.\n" \
    "    d, database: Read and manage TPBench database.\n" \
    "    k, kernel: Kernel management (refreshes kernel records in the workspace).\n" \
    "        list, ls: List integrated kernels (Kernel, KernelID, Description).\n" \
    "        init: Initialize out-of-tree kernel project from templates.\n" \
    "            --dir <path> --kernel <name>\n" \
    "        build: Build out-of-tree kernel and install libtpbk_<name>.so.\n" \
    "            --dir <path> --kernel <name> [--tpb-home <path>] [-D...] [--cc ...] [--cflags ...]\n" \
    "            TPB home for build: --tpb-home, then $TPB_HOME, then $HOME/.tpbench.\n" \
    "        set, get, backup-inactive: Kernel metadata and compile history.\n" \
    "    help: Print help message for an object and exit.\n"

/**
 * @brief Print overall tpbcli help message to stdout.
 */
void tpbcli_print_help_total(void);

#endif /* TPBCLI_HELP_DOC_H */
