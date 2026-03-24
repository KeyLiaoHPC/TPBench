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

# Set default optimization to -O2
add_compile_options(-O2)

# SIMD and parallelization options
option(TPB_USE_AVX512 "Enable AVX-512 SIMD instructions" OFF)
option(TPB_USE_AVX2 "Enable AVX2 SIMD instructions" OFF)
option(TPB_USE_KP_SVE "Enable ARM SVE SIMD instructions" OFF)
option(TPB_USE_OPENMP "Enable OpenMP parallelization" OFF)

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
if(TPB_USE_OPENMP)
    find_package(OpenMP)
    if(OpenMP_C_FOUND)
        add_compile_definitions(TPB_USE_OPENMP)
        message(STATUS "TPB_USE_OPENMP enabled")
    else()
        message(WARNING "OpenMP not found, TPB_USE_OPENMP disabled")
    endif()
endif()

# MPI support (optional, for MPI kernels only - not linked to libtpbench.so)
# Usage:
#   -DTPB_USE_MPI=OFF or -DTPB_USE_MPI=FALSE or -DTPB_USE_MPI=0 or leave unset → MPI disabled
#   -DTPB_USE_MPI=ON → Search MPI in system default locations
#   -DTPB_USE_MPI=/path/to/mpi → Search MPI in the specified directory
set(TPB_USE_MPI "" CACHE STRING "MPI support: OFF to disable, ON for system default, or specify MPI installation path")

# Normalize TPB_USE_MPI to uppercase for case-insensitive comparison
string(TOUPPER "${TPB_USE_MPI}" TPB_USE_MPI_UPPER)

# Check if MPI should be enabled (ON or a path)
if(TPB_USE_MPI_UPPER STREQUAL "ON")
    # Case 2: TPB_USE_MPI=ON - search in system default locations
    find_package(MPI REQUIRED)
    if(MPI_C_FOUND)
        add_compile_definitions(TPB_USE_MPI)
        message(STATUS "MPI enabled (system default)")
    else()
        message(FATAL_ERROR "MPI not found in system default locations")
    endif()
elseif(TPB_USE_MPI_UPPER STREQUAL "OFF" OR TPB_USE_MPI_UPPER STREQUAL "FALSE" OR TPB_USE_MPI_UPPER STREQUAL "0" OR TPB_USE_MPI STREQUAL "")
    # Case 1: MPI disabled (empty, 0, OFF, FALSE)
    message(STATUS "MPI disabled")
else()
    # Case 3: TPB_USE_MPI=/path/to/mpi - search in specified directory
    set(MPI_ROOT ${TPB_USE_MPI})
    find_package(MPI REQUIRED)
    if(MPI_C_FOUND)
        add_compile_definitions(TPB_USE_MPI)
        message(STATUS "MPI enabled from: ${TPB_USE_MPI}")
    else()
        message(FATAL_ERROR "MPI not found at ${TPB_USE_MPI}")
    endif()
endif()

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
    src/tpbcli-benchmark.c
    src/tpbcli-help.c
    src/tpb-bench-yaml.c
    src/tpb-bench-score.c
    src/tpbcli-database.c
    src/tpbcli-database-ls.c
    src/tpbcli-database-dump.c
)
target_include_directories(tpbcli PRIVATE src)
target_link_libraries(tpbcli PRIVATE tpbench ${TPB_BUILT_KERNEL_LIBS} m)
tpb_set_install_rpath_tpbench_exe(tpbcli)

# ============================================================================
# ROCm/HIP GPU Support
# ============================================================================
option(TPB_USE_ROCM "Enable ROCm/HIP GPU support" OFF)

if(TPB_USE_ROCM)
    # Enable HIP language support (requires CMake 3.21+)
    cmake_minimum_required(VERSION 3.21)
    enable_language(HIP)

    # Find HIP package
    find_package(hip REQUIRED)
    if(hip_FOUND)
        add_compile_definitions(TPB_USE_ROCM)
        message(STATUS "ROCm/HIP enabled")
        message(STATUS "  HIP version: ${hip_VERSION}")
    else()
        message(FATAL_ERROR "HIP not found but TPB_USE_ROCM is ON")
    endif()

    include(${CMAKE_SOURCE_DIR}/cmake/TPBenchGpuKernelsRocm.cmake)
endif()

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
