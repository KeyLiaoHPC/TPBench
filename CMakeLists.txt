cmake_minimum_required(VERSION 3.16) # Require at least CMake 3.16
project(TPBench C CXX) # Project name and languages (CXX needed for ROCm)

# Use C11 and require it
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Set default optimization to -O2
add_compile_options(-O2)

# SIMD and parallelization options
option(TPB_USE_AVX512 "Enable AVX-512 SIMD instructions" OFF)
option(TPB_USE_AVX2 "Enable AVX2 SIMD instructions" OFF)
option(TPB_USE_KP_SVE "Enable ARM SVE SIMD instructions" OFF)
option(TPB_USE_OPENMP "Enable OpenMP parallelization" ON)

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

# Triad kernel as shared library (renamed to tpbk_triad for PLI consistency)
add_library(tpbk_triad SHARED src/kernels/simple/tpbk_triad.c)
target_include_directories(tpbk_triad PRIVATE src)
target_link_libraries(tpbk_triad PRIVATE tpbench m)
if(OpenMP_C_FOUND)
    target_link_libraries(tpbk_triad PRIVATE OpenMP::OpenMP_C)
endif()

# Stream kernel as shared library
add_library(tpbk_stream SHARED src/kernels/simple/tpbk_stream.c)
target_include_directories(tpbk_stream PRIVATE src)
target_link_libraries(tpbk_stream PRIVATE tpbench m)
if(OpenMP_C_FOUND)
    target_link_libraries(tpbk_stream PRIVATE OpenMP::OpenMP_C)
endif()

# Staxpy kernel as shared library
add_library(tpbk_staxpy SHARED src/kernels/simple/tpbk_staxpy.c)
target_include_directories(tpbk_staxpy PRIVATE src)
target_link_libraries(tpbk_staxpy PRIVATE tpbench m)
if(OpenMP_C_FOUND)
    target_link_libraries(tpbk_staxpy PRIVATE OpenMP::OpenMP_C)
endif()

# Striad kernel as shared library
add_library(tpbk_striad SHARED src/kernels/simple/tpbk_striad.c)
target_include_directories(tpbk_striad PRIVATE src)
target_link_libraries(tpbk_striad PRIVATE tpbench m)
if(OpenMP_C_FOUND)
    target_link_libraries(tpbk_striad PRIVATE OpenMP::OpenMP_C)
endif()

# Scale kernel as shared library
add_library(tpbk_scale SHARED src/kernels/simple/tpbk_scale.c)
target_include_directories(tpbk_scale PRIVATE src)
target_link_libraries(tpbk_scale PRIVATE tpbench m)
if(OpenMP_C_FOUND)
    target_link_libraries(tpbk_scale PRIVATE OpenMP::OpenMP_C)
endif()

# AXPY kernel as shared library
add_library(tpbk_axpy SHARED src/kernels/simple/tpbk_axpy.c)
target_include_directories(tpbk_axpy PRIVATE src)
target_link_libraries(tpbk_axpy PRIVATE tpbench m)
if(OpenMP_C_FOUND)
    target_link_libraries(tpbk_axpy PRIVATE OpenMP::OpenMP_C)
endif()

# Repeat Triad kernel as shared library
add_library(tpbk_rtriad SHARED src/kernels/simple/tpbk_rtriad.c)
target_include_directories(tpbk_rtriad PRIVATE src)
target_link_libraries(tpbk_rtriad PRIVATE tpbench m)
if(OpenMP_C_FOUND)
    target_link_libraries(tpbk_rtriad PRIVATE OpenMP::OpenMP_C)
endif()

# Sum kernel as shared library
add_library(tpbk_sum SHARED src/kernels/simple/tpbk_sum.c)
target_include_directories(tpbk_sum PRIVATE src)
target_link_libraries(tpbk_sum PRIVATE tpbench m)
if(OpenMP_C_FOUND)
    target_link_libraries(tpbk_sum PRIVATE OpenMP::OpenMP_C)
endif()

# Stream MPI kernel (only built when MPI is enabled)
if(MPI_C_FOUND)
    add_library(tpbk_stream_mpi SHARED src/kernels/simple/tpbk_stream_mpi.c)
    target_include_directories(tpbk_stream_mpi PRIVATE src)
    target_link_libraries(tpbk_stream_mpi PRIVATE tpbench m MPI::MPI_C)
    if(OpenMP_C_FOUND)
        target_link_libraries(tpbk_stream_mpi PRIVATE OpenMP::OpenMP_C)
    endif()

    # Scale MPI kernel
    add_library(tpbk_scale_mpi SHARED src/kernels/simple/tpbk_scale_mpi.c)
    target_include_directories(tpbk_scale_mpi PRIVATE src)
    target_link_libraries(tpbk_scale_mpi PRIVATE tpbench m MPI::MPI_C)
    if(OpenMP_C_FOUND)
        target_link_libraries(tpbk_scale_mpi PRIVATE OpenMP::OpenMP_C)
    endif()

    # AXPY MPI kernel
    add_library(tpbk_axpy_mpi SHARED src/kernels/simple/tpbk_axpy_mpi.c)
    target_include_directories(tpbk_axpy_mpi PRIVATE src)
    target_link_libraries(tpbk_axpy_mpi PRIVATE tpbench m MPI::MPI_C)
    if(OpenMP_C_FOUND)
        target_link_libraries(tpbk_axpy_mpi PRIVATE OpenMP::OpenMP_C)
    endif()

    # Repeat Triad MPI kernel
    add_library(tpbk_rtriad_mpi SHARED src/kernels/simple/tpbk_rtriad_mpi.c)
    target_include_directories(tpbk_rtriad_mpi PRIVATE src)
    target_link_libraries(tpbk_rtriad_mpi PRIVATE tpbench m MPI::MPI_C)
    if(OpenMP_C_FOUND)
        target_link_libraries(tpbk_rtriad_mpi PRIVATE OpenMP::OpenMP_C)
    endif()

    # Sum MPI kernel
    add_library(tpbk_sum_mpi SHARED src/kernels/simple/tpbk_sum_mpi.c)
    target_include_directories(tpbk_sum_mpi PRIVATE src)
    target_link_libraries(tpbk_sum_mpi PRIVATE tpbench m MPI::MPI_C)
    if(OpenMP_C_FOUND)
        target_link_libraries(tpbk_sum_mpi PRIVATE OpenMP::OpenMP_C)
    endif()
endif()

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
)
target_include_directories(tpbcli PRIVATE src)
target_link_libraries(tpbcli PRIVATE tpbench tpbk_triad tpbk_stream tpbk_staxpy tpbk_striad tpbk_scale tpbk_axpy tpbk_rtriad tpbk_sum m)

# PLI Executables (.tpbx) for triad and stream kernels
add_executable(tpbk_triad.tpbx src/kernels/simple/tpbk_triad_main.c)
target_include_directories(tpbk_triad.tpbx PRIVATE src)
target_link_libraries(tpbk_triad.tpbx PRIVATE tpbench tpbk_triad m)
if(OpenMP_C_FOUND)
    target_link_libraries(tpbk_triad.tpbx PRIVATE OpenMP::OpenMP_C)
endif()

add_executable(tpbk_stream.tpbx src/kernels/simple/tpbk_stream_main.c)
target_include_directories(tpbk_stream.tpbx PRIVATE src)
target_link_libraries(tpbk_stream.tpbx PRIVATE tpbench tpbk_stream m)
if(OpenMP_C_FOUND)
    target_link_libraries(tpbk_stream.tpbx PRIVATE OpenMP::OpenMP_C)
endif()

add_executable(tpbk_staxpy.tpbx src/kernels/simple/tpbk_staxpy_main.c)
target_include_directories(tpbk_staxpy.tpbx PRIVATE src)
target_link_libraries(tpbk_staxpy.tpbx PRIVATE tpbench tpbk_staxpy m)
if(OpenMP_C_FOUND)
    target_link_libraries(tpbk_staxpy.tpbx PRIVATE OpenMP::OpenMP_C)
endif()

add_executable(tpbk_striad.tpbx src/kernels/simple/tpbk_striad_main.c)
target_include_directories(tpbk_striad.tpbx PRIVATE src)
target_link_libraries(tpbk_striad.tpbx PRIVATE tpbench tpbk_striad m)
if(OpenMP_C_FOUND)
    target_link_libraries(tpbk_striad.tpbx PRIVATE OpenMP::OpenMP_C)
endif()

# Scale PLI executable
add_executable(tpbk_scale.tpbx src/kernels/simple/tpbk_scale_main.c)
target_include_directories(tpbk_scale.tpbx PRIVATE src)
target_link_libraries(tpbk_scale.tpbx PRIVATE tpbench tpbk_scale m)
if(OpenMP_C_FOUND)
    target_link_libraries(tpbk_scale.tpbx PRIVATE OpenMP::OpenMP_C)
endif()

# AXPY PLI executable
add_executable(tpbk_axpy.tpbx src/kernels/simple/tpbk_axpy_main.c)
target_include_directories(tpbk_axpy.tpbx PRIVATE src)
target_link_libraries(tpbk_axpy.tpbx PRIVATE tpbench tpbk_axpy m)
if(OpenMP_C_FOUND)
    target_link_libraries(tpbk_axpy.tpbx PRIVATE OpenMP::OpenMP_C)
endif()

# Repeat Triad PLI executable
add_executable(tpbk_rtriad.tpbx src/kernels/simple/tpbk_rtriad_main.c)
target_include_directories(tpbk_rtriad.tpbx PRIVATE src)
target_link_libraries(tpbk_rtriad.tpbx PRIVATE tpbench tpbk_rtriad m)
if(OpenMP_C_FOUND)
    target_link_libraries(tpbk_rtriad.tpbx PRIVATE OpenMP::OpenMP_C)
endif()

# Sum PLI executable
add_executable(tpbk_sum.tpbx src/kernels/simple/tpbk_sum_main.c)
target_include_directories(tpbk_sum.tpbx PRIVATE src)
target_link_libraries(tpbk_sum.tpbx PRIVATE tpbench tpbk_sum m)
if(OpenMP_C_FOUND)
    target_link_libraries(tpbk_sum.tpbx PRIVATE OpenMP::OpenMP_C)
endif()

# Stream MPI PLI executable (only built when MPI is enabled)
if(MPI_C_FOUND)
    add_executable(tpbk_stream_mpi.tpbx src/kernels/simple/tpbk_stream_mpi_main.c)
    target_include_directories(tpbk_stream_mpi.tpbx PRIVATE src)
    target_link_libraries(tpbk_stream_mpi.tpbx PRIVATE tpbench tpbk_stream_mpi m MPI::MPI_C)
    if(OpenMP_C_FOUND)
        target_link_libraries(tpbk_stream_mpi.tpbx PRIVATE OpenMP::OpenMP_C)
    endif()

    # Scale MPI PLI executable
    add_executable(tpbk_scale_mpi.tpbx src/kernels/simple/tpbk_scale_mpi_main.c)
    target_include_directories(tpbk_scale_mpi.tpbx PRIVATE src)
    target_link_libraries(tpbk_scale_mpi.tpbx PRIVATE tpbench tpbk_scale_mpi m MPI::MPI_C)
    if(OpenMP_C_FOUND)
        target_link_libraries(tpbk_scale_mpi.tpbx PRIVATE OpenMP::OpenMP_C)
    endif()

    # AXPY MPI PLI executable
    add_executable(tpbk_axpy_mpi.tpbx src/kernels/simple/tpbk_axpy_mpi_main.c)
    target_include_directories(tpbk_axpy_mpi.tpbx PRIVATE src)
    target_link_libraries(tpbk_axpy_mpi.tpbx PRIVATE tpbench tpbk_axpy_mpi m MPI::MPI_C)
    if(OpenMP_C_FOUND)
        target_link_libraries(tpbk_axpy_mpi.tpbx PRIVATE OpenMP::OpenMP_C)
    endif()

    # Repeat Triad MPI PLI executable
    add_executable(tpbk_rtriad_mpi.tpbx src/kernels/simple/tpbk_rtriad_mpi_main.c)
    target_include_directories(tpbk_rtriad_mpi.tpbx PRIVATE src)
    target_link_libraries(tpbk_rtriad_mpi.tpbx PRIVATE tpbench tpbk_rtriad_mpi m MPI::MPI_C)
    if(OpenMP_C_FOUND)
        target_link_libraries(tpbk_rtriad_mpi.tpbx PRIVATE OpenMP::OpenMP_C)
    endif()

    # Sum MPI PLI executable
    add_executable(tpbk_sum_mpi.tpbx src/kernels/simple/tpbk_sum_mpi_main.c)
    target_include_directories(tpbk_sum_mpi.tpbx PRIVATE src)
    target_link_libraries(tpbk_sum_mpi.tpbx PRIVATE tpbench tpbk_sum_mpi m MPI::MPI_C)
    if(OpenMP_C_FOUND)
        target_link_libraries(tpbk_sum_mpi.tpbx PRIVATE OpenMP::OpenMP_C)
    endif()
endif()

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

    # Roofline ROCm kernel as shared library
    add_library(tpbk_roofline_rocm SHARED
        src/kernels/rocm/tpbk_roofline_rocm.hip)
    target_include_directories(tpbk_roofline_rocm PRIVATE src)
    target_link_libraries(tpbk_roofline_rocm PRIVATE tpbench hip::device m)
    set_target_properties(tpbk_roofline_rocm PROPERTIES
        HIP_SEPARABLE_COMPILATION ON
        POSITION_INDEPENDENT_CODE ON)
    # Enable debug logging if TPB_SHOW_DEBUG is ON
    if(TPB_SHOW_DEBUG)
        target_compile_definitions(tpbk_roofline_rocm PRIVATE TPB_K_DEBUG=1)
        message(STATUS "TPB_K_DEBUG enabled for tpbk_roofline_rocm")
    endif()

    # Roofline ROCm PLI executable (uses regular C++ compiler, links HIP runtime)
    add_executable(tpbk_roofline_rocm.tpbx
        src/kernels/rocm/tpbk_roofline_rocm_main.cpp)
    target_include_directories(tpbk_roofline_rocm.tpbx PRIVATE src)
    target_link_libraries(tpbk_roofline_rocm.tpbx PRIVATE
        tpbench tpbk_roofline_rocm hip::host m)
    set_target_properties(tpbk_roofline_rocm.tpbx PROPERTIES
        LINKER_LANGUAGE CXX
        CXX_STANDARD 11)
    # Enable debug logging if TPB_SHOW_DEBUG is ON
    if(TPB_SHOW_DEBUG)
        target_compile_definitions(tpbk_roofline_rocm.tpbx PRIVATE TPB_K_DEBUG=1)
        message(STATUS "TPB_K_DEBUG enabled for tpbk_roofline_rocm.tpbx")
    endif()
endif()

# Tests
option(BUILD_TESTING "Build test targets" ON)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

# Install targets
install(TARGETS tpbench tpbk_triad tpbk_stream tpbk_staxpy tpbk_striad tpbk_scale tpbk_axpy tpbk_rtriad tpbk_sum
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(TARGETS tpbcli tpbk_triad.tpbx tpbk_stream.tpbx tpbk_staxpy.tpbx tpbk_striad.tpbx tpbk_scale.tpbx tpbk_axpy.tpbx tpbk_rtriad.tpbx tpbk_sum.tpbx
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# Conditional install for MPI targets
if(MPI_C_FOUND)
    install(TARGETS tpbk_stream_mpi tpbk_scale_mpi tpbk_axpy_mpi tpbk_rtriad_mpi tpbk_sum_mpi
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(TARGETS tpbk_stream_mpi.tpbx tpbk_scale_mpi.tpbx tpbk_axpy_mpi.tpbx tpbk_rtriad_mpi.tpbx tpbk_sum_mpi.tpbx
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

# Conditional install for ROCm targets
if(TPB_USE_ROCM)
    install(TARGETS tpbk_roofline_rocm
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(TARGETS tpbk_roofline_rocm.tpbx
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()


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


# Copy YAML configuration files to build/etc directory
add_custom_target(copy_yaml_files ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/etc
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/setup/yaml ${CMAKE_BINARY_DIR}/etc
    COMMENT "Copying YAML configuration files to build/etc"
)
