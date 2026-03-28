# TPBenchKernelRegistry.cmake
#
# Single source for CPU and ROCm kernel catalogs (build + tpb_cmake_help).
#
# CPU: each entry NAME|DEFAULT_TAGS|EXTRA_LINK_LIBS|CONDITION
#   CONDITION: empty = always eligible; else CMake variable (e.g. MPI_C_FOUND).
#
# ROCm: each entry NAME|DEFAULT_TAGS|PREREQ_TEXT|BACKEND|HIP_LIB_SOURCE|PLI_MAIN_SOURCE
#   Paths relative to CMAKE_SOURCE_DIR. BACKEND must be "rocm".

set(TPB_CPU_KERNEL_DEFS
    "stream|default,bandwidth||"
    "triad|default,bandwidth||"
    "scale|default,bandwidth||"
    "axpy|default,bandwidth||"
    "rtriad|default,bandwidth||"
    "sum|default,bandwidth||"
    "staxpy|bandwidth,stanza||"
    "striad|bandwidth,stanza||"
    "stream_mpi|bandwidth,mpi|MPI::MPI_C|MPI_C_FOUND"
    # "scale_mpi|bandwidth,mpi|MPI::MPI_C|MPI_C_FOUND"
    # "axpy_mpi|bandwidth,mpi|MPI::MPI_C|MPI_C_FOUND"
    # "rtriad_mpi|bandwidth,mpi|MPI::MPI_C|MPI_C_FOUND"
    # "sum_mpi|bandwidth,mpi|MPI::MPI_C|MPI_C_FOUND"
)

set(TPB_ROCM_KERNEL_DEFS
    "roofline_rocm|gpu,roofline,rocm|HIP toolchain when selected|rocm|src/kernels/rocm/tpbk_roofline_rocm.hip|src/kernels/rocm/tpbk_roofline_rocm_main.cpp"
)
