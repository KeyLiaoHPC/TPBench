cmake_minimum_required(VERSION 3.16) # Require at least CMake 3.16
project(TPBench C CXX) # Project name and languages (CXX needed for ROCm)

# Optional workspace root: seeds default CMAKE_INSTALL_PREFIX when prefix is CMake default.
# Precedence at install time: cmake --install --prefix > configured CMAKE_INSTALL_PREFIX.
set(TPB_WORKSPACE "$ENV{TPB_WORKSPACE}" CACHE PATH
    "Workspace root; default install prefix when CMAKE_INSTALL_PREFIX is CMake default (else /opt/tpbench)")
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    if(TPB_WORKSPACE AND NOT TPB_WORKSPACE STREQUAL "")
        set(CMAKE_INSTALL_PREFIX "${TPB_WORKSPACE}" CACHE PATH "Install path prefix" FORCE)
    else()
        set(CMAKE_INSTALL_PREFIX "/opt/tpbench" CACHE PATH "Install path prefix" FORCE)
    endif()
endif()

# Use C11 and require it
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Default -O2 is applied per non-kernel target (kernels use TPB_KERNEL_*FLAGS or -O2).

# SIMD and parallelization options
option(TPB_USE_AVX512 "Enable AVX-512 SIMD instructions" OFF)
option(TPB_USE_AVX2 "Enable AVX2 SIMD instructions" OFF)
option(TPB_USE_KP_SVE "Enable ARM SVE SIMD instructions" OFF)
option(TPB_ENABLE_OPENMP "Add OpenMP compile/link to built kernel targets" OFF)

# Debug options
option(TPB_SHOW_DEBUG "Enable kernel debug logging (TPB_K_DEBUG)" OFF)

# Apply SIMD compile definitions
if(TPB_USE_AVX512)
    add_compile_definitions(TPB_USE_AVX512)
    message(STATUS "TPB_USE_AVX512 enabled")
endif()
if(TPB_USE_AVX2)
    add_compile_definitions(TPB_USE_AVX2)
    message(STATUS "TPB_USE_AVX2 enabled")
endif()
if(TPB_USE_KP_SVE)
    add_compile_definitions(TPB_USE_KP_SVE)
    message(STATUS "TPB_USE_KP_SVE enabled")
endif()

set(OpenMP_C_FOUND FALSE)
set(OpenMP_CXX_FOUND FALSE)
if(TPB_ENABLE_OPENMP)
    find_package(OpenMP)
    if(OpenMP_C_FOUND OR OpenMP_CXX_FOUND)
        message(STATUS "TPB_ENABLE_OPENMP: OpenMP available for kernel targets")
    else()
        message(WARNING "OpenMP not found; TPB_ENABLE_OPENMP has no effect")
    endif()
endif()

# MPI: resolved only when an MPI-conditioned CPU kernel is selected (see scan below).
set(TPB_MPI_PATH "" CACHE PATH "MPI install root when an MPI kernel is selected (empty = auto-detect)")
set(TPB_ROCM_PATH "" CACHE PATH "ROCm root when a rocm-tagged GPU kernel is selected (empty = auto-detect)")

# Set output directories for build artifacts
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Set install directories
set(CMAKE_INSTALL_BINDIR bin)
set(CMAKE_INSTALL_LIBDIR lib)
set(CMAKE_INSTALL_INCLUDEDIR include)

# TPB_DIR option for kernel discovery
# If not set, defaults to CMAKE_BINARY_DIR (build directory)
set(TPB_DIR "" CACHE STRING "TPBench installation directory for kernel discovery")
if(TPB_DIR STREQUAL "")
    set(TPB_DIR_RESOLVED "${CMAKE_BINARY_DIR}")
else()
    set(TPB_DIR_RESOLVED "${TPB_DIR}")
endif()
message(STATUS "TPB_DIR resolved to: ${TPB_DIR_RESOLVED}")

include(${CMAKE_SOURCE_DIR}/cmake/TPBenchKernelRegistry.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/TPBenchKernelSelect.cmake)

set(TPB_KERNELS "default" CACHE STRING "Kernel selection: comma-separated names, 'all', or 'default'")
set(TPB_KERNEL_TAGS "" CACHE STRING "Comma-separated tags to select kernels (union with TPB_KERNELS)")

string(REPLACE "," ";" _tpb_sel_names "${TPB_KERNELS}")
string(REPLACE "," ";" _tpb_sel_tags "${TPB_KERNEL_TAGS}")

set(TPB_NEED_MPI FALSE)
foreach(_def IN LISTS TPB_CPU_KERNEL_DEFS)
    string(REPLACE "|" ";" _parts "${_def}")
    list(GET _parts 0 _scan_kname)
    list(GET _parts 1 _scan_ktags_str)
    list(GET _parts 3 _scan_kcond)
    tpb_kernel_should_build("${_scan_kname}" "${_scan_ktags_str}" "${_tpb_sel_names}" "${_tpb_sel_tags}" _scan_match)
    if(_scan_match AND _scan_kcond STREQUAL "MPI_C_FOUND")
        set(TPB_NEED_MPI TRUE)
    endif()
endforeach()

set(TPB_NEED_HIP FALSE)
foreach(_def IN LISTS TPB_ROCM_KERNEL_DEFS)
    string(REPLACE "|" ";" _parts "${_def}")
    list(GET _parts 0 _scan_rk_name)
    list(GET _parts 1 _scan_rk_tags_str)
    string(REPLACE "," ";" _scan_rk_taglist "${_scan_rk_tags_str}")
    tpb_kernel_should_build("${_scan_rk_name}" "${_scan_rk_tags_str}" "${_tpb_sel_names}" "${_tpb_sel_tags}" _scan_rk_match)
    if(_scan_rk_match AND "rocm" IN_LIST _scan_rk_taglist)
        set(TPB_NEED_HIP TRUE)
    endif()
endforeach()

if(TPB_NEED_MPI)
    if(TPB_MPI_PATH AND IS_DIRECTORY "${TPB_MPI_PATH}")
        set(MPI_ROOT "${TPB_MPI_PATH}")
    endif()
    find_package(MPI REQUIRED)
    message(STATUS "MPI enabled for selected MPI kernel(s)")
endif()

set(TPB_ROCM_KERNEL_LIBS "")
set(TPB_ROCM_KERNEL_EXES "")

if(TPB_NEED_HIP)
    cmake_minimum_required(VERSION 3.21)
    set(_tpb_rocm_root "")
    if(TPB_ROCM_PATH AND IS_DIRECTORY "${TPB_ROCM_PATH}")
        set(_tpb_rocm_root "${TPB_ROCM_PATH}")
    elseif(DEFINED ENV{ROCM_PATH} AND IS_DIRECTORY "$ENV{ROCM_PATH}")
        set(_tpb_rocm_root "$ENV{ROCM_PATH}")
    elseif(EXISTS /opt/rocm AND IS_DIRECTORY /opt/rocm)
        set(_tpb_rocm_root /opt/rocm)
    else()
        find_program(_tpb_hipcc NAMES hipcc
            PATHS
            /opt/rocm/bin
            /usr/local/rocm/bin
            ENV PATH)
        if(_tpb_hipcc)
            get_filename_component(_tpb_hip_bin "${_tpb_hipcc}" DIRECTORY)
            get_filename_component(_tpb_rocm_root "${_tpb_hip_bin}" DIRECTORY)
        endif()
    endif()
    if(NOT _tpb_rocm_root OR NOT IS_DIRECTORY "${_tpb_rocm_root}")
        message(FATAL_ERROR "Selected GPU kernel requires ROCm/HIP but no toolchain found. Set TPB_ROCM_PATH or install ROCm.")
    endif()
    list(PREPEND CMAKE_PREFIX_PATH "${_tpb_rocm_root}")
    set(CMAKE_HIP_COMPILER_ROCM_ROOT "${_tpb_rocm_root}")
    enable_language(HIP)
    find_package(hip REQUIRED)
    message(STATUS "ROCm/HIP enabled for selected GPU kernel(s) (root: ${_tpb_rocm_root})")
    message(STATUS "  HIP version: ${hip_VERSION}")
endif()

# Timer-related sources (absolute paths for use in sub-CMakeLists)
set(TPB_TIMER_SOURCES
    ${CMAKE_SOURCE_DIR}/src/timers/clock_gettime.c
)

# Add TSC timer for x86_64/amd64 platforms
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
    list(APPEND TPB_TIMER_SOURCES ${CMAKE_SOURCE_DIR}/src/timers/tsc_asym.c)
endif()

# Core library (libtpbench.so) defined in sub-CMakeLists
add_subdirectory(src/corelib)

include(${CMAKE_SOURCE_DIR}/cmake/TPBenchInstallRpath.cmake)

# Kernels (.so + .tpbx) with name/tag-based selection
add_subdirectory(src/kernels)

if(TPB_NEED_HIP)
    include(${CMAKE_SOURCE_DIR}/cmake/TPBenchGpuKernelsRocm.cmake)
endif()

include(${CMAKE_SOURCE_DIR}/cmake/TPBenchCmakeHelp.cmake)

add_custom_target(tpb_cmake_help
    COMMAND ${CMAKE_COMMAND} -E cat "${CMAKE_BINARY_DIR}/tpb_cmake_help.txt"
    COMMENT "TPBench CMake options and kernel catalog"
    VERBATIM)

# Main CLI executable and subcommands
add_executable(tpbcli
    src/tpbcli.c
    src/tpbcli-run.c
    src/tpbcli-run-dim.c
    src/tpbcli-list.c
    src/tpbcli-kernel.c
    src/tpbcli-benchmark.c
    src/tpbcli-help.c
    src/tpb-bench-yaml.c
    src/tpb-bench-score.c
    src/tpbcli-database.c
    src/tpbcli-database-ls.c
    src/tpbcli-database-dump.c
)
target_include_directories(tpbcli PRIVATE src)
target_compile_options(tpbcli PRIVATE -O2)
target_link_libraries(tpbcli PRIVATE tpbench ${TPB_BUILT_KERNEL_LIBS} m)
tpb_set_install_rpath_tpbench_exe(tpbcli)

add_custom_target(tpb_build_kernel
    DEPENDS tpbench ${TPB_BUILT_KERNEL_LIBS} ${TPB_BUILT_KERNEL_EXES}
            ${TPB_ROCM_KERNEL_LIBS} ${TPB_ROCM_KERNEL_EXES}
    COMMENT "Build libtpbench.so and selected kernel .so / .tpbx only")

add_custom_target(tpb_install_kernel
    DEPENDS tpb_build_kernel
    COMMAND ${CMAKE_COMMAND} --install "${CMAKE_BINARY_DIR}"
            --component tpbench_kernels --prefix "${CMAKE_INSTALL_PREFIX}"
    COMMENT "Install kernel shared libraries and .tpbx (COMPONENT tpbench_kernels)")

# Tests
option(BUILD_TESTING "Build test targets" ON)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

# Workspace layout under install root (DESTDIR + CMAKE_INSTALL_PREFIX at install time).
# Creates dirs only; does not touch rawdb files. etc/config.json only if missing.
install(CODE "
  if(NOT \"\$ENV{DESTDIR}\" STREQUAL \"\")
    set(_tpb_install_root \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}\")
  else()
    set(_tpb_install_root \"\${CMAKE_INSTALL_PREFIX}\")
  endif()
  file(MAKE_DIRECTORY
    \"\${_tpb_install_root}/etc\"
    \"\${_tpb_install_root}/rawdb/task_batch\"
    \"\${_tpb_install_root}/rawdb/kernel\"
    \"\${_tpb_install_root}/rawdb/task\"
    \"\${_tpb_install_root}/rawdb/log\")
  if(NOT EXISTS \"\${_tpb_install_root}/etc/config.json\")
    file(WRITE \"\${_tpb_install_root}/etc/config.json\"
      \"{\\n    \\\"name\\\": \\\"default\\\"\\n}\\n\")
  endif()
")

# Install core library and CLI
install(TARGETS tpbench
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(TARGETS tpbcli
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# Kernel install rules are handled inside src/kernels/CMakeLists.txt
# ROCm kernel install rules are in cmake/TPBenchGpuKernelsRocm.cmake

# ============================================================================
# Generate and Install Flat tpbench.h for End-User Distribution
# ============================================================================

# Read source header files
file(READ ${CMAKE_SOURCE_DIR}/src/include/tpb-unitdefs.h UNITDEFS_CONTENT)
file(READ ${CMAKE_SOURCE_DIR}/src/include/tpb-public.h PUBLIC_CONTENT)

# Remove the #include "tpb-unitdefs.h" line from tpb-public.h content
string(REPLACE "#include \"tpb-unitdefs.h\"\n" "" PUBLIC_CONTENT "${PUBLIC_CONTENT}")
string(REPLACE "#include \"tpb-unitdefs.h\"" "" PUBLIC_CONTENT "${PUBLIC_CONTENT}")

# Create output directory for generated headers
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/include)

# Generate the merged flat header using configure_file with @ONLY
configure_file(
    ${CMAKE_SOURCE_DIR}/src/include/tpbench.h.in
    ${CMAKE_BINARY_DIR}/include/tpbench.h
    @ONLY
)

# Install only the generated flat tpbench.h (not separate tpb-unitdefs.h or tpb-public.h)
install(FILES ${CMAKE_BINARY_DIR}/include/tpbench.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Install CMake package config for out-of-tree kernel builds
configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/TPBenchConfig.cmake.in
    ${CMAKE_BINARY_DIR}/cmake/TPBenchConfig.cmake
    @ONLY
)
install(FILES ${CMAKE_BINARY_DIR}/cmake/TPBenchConfig.cmake
              ${CMAKE_SOURCE_DIR}/cmake/TPBenchKernel.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/TPBench)


# Copy YAML configuration files to build/etc directory
add_custom_target(copy_yaml_files ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/etc
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/setup/yaml ${CMAKE_BINARY_DIR}/etc
    COMMENT "Copying YAML configuration files to build/etc"
)
