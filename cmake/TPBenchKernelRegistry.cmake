# TPBenchKernelRegistry.cmake
#
# CPU kernel name/tags/path: src/kernels/kernel_list.cmake.in
# CPU link/condition overrides below (only kernels needing special handling).
#
# ROCm: each entry NAME|DEFAULT_TAGS|PREREQ_TEXT|BACKEND|HIP_LIB_SOURCE|PLI_ENTRY_SOURCE
#   Paths relative to CMAKE_SOURCE_DIR. BACKEND must be "rocm".

set(TPB_CPU_KERNEL_LINK_DEFS
    "stream_mpi|MPI::MPI_C|MPI_C_FOUND"
    # "scale_mpi|MPI::MPI_C|MPI_C_FOUND"
    # "axpy_mpi|MPI::MPI_C|MPI_C_FOUND"
    # "rtriad_mpi|MPI::MPI_C|MPI_C_FOUND"
    # "sum_mpi|MPI::MPI_C|MPI_C_FOUND"
)

set(TPB_ROCM_KERNEL_DEFS
    "roofline_rocm|gpu,roofline,rocm|HIP toolchain when selected|rocm|src/kernels/rocm/tpbk_roofline_rocm.hip|src/kernels/rocm/tpbk_roofline_rocm_main.cpp"
)
