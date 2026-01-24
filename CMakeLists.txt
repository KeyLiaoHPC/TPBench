cmake_minimum_required(VERSION 3.16) # Require at least CMake 3.16
project(TPBench C) # Project name and language

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
set(TPB_USE_MPI "" CACHE PATH "Path to MPI installation (leave empty to disable)")
if(NOT TPB_USE_MPI STREQUAL "")
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

# Main TPBench sources
set(TPB_SOURCES
    src/tpb-driver.c
    src/tpb-io.c
    src/tpb-stat.c
    src/tpb-unitcast.c
    src/tpb-impl.c
    src/tpb-argp.c
    src/tpb-argp-dim.c
    src/tpb-dynloader.c
)

# Timer-related sources
set(TPB_TIMER_SOURCES
    src/timers/clock_gettime.c
)

# Add TSC timer for x86_64/amd64 platforms
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
    list(APPEND TPB_TIMER_SOURCES src/timers/tsc_asym.c)
endif()

# Shared library for core TPBench implementation
add_library(tpbench SHARED ${TPB_SOURCES} ${TPB_TIMER_SOURCES})
target_include_directories(tpbench PUBLIC src src/include)
target_link_libraries(tpbench PUBLIC m dl)
target_compile_definitions(tpbench PRIVATE TPB_DIR="${TPB_DIR_RESOLVED}")

# Triad kernel as shared library (renamed to tpbk_triad for PLI consistency)
add_library(tpbk_triad SHARED src/kernels/simple/tpbk_triad.c)
target_include_directories(tpbk_triad PRIVATE src src/include)
target_link_libraries(tpbk_triad PRIVATE tpbench m)
if(OpenMP_C_FOUND)
    target_link_libraries(tpbk_triad PRIVATE OpenMP::OpenMP_C)
endif()

# Stream kernel as shared library
add_library(tpbk_stream SHARED src/kernels/simple/tpbk_stream.c)
target_include_directories(tpbk_stream PRIVATE src src/include)
target_link_libraries(tpbk_stream PRIVATE tpbench m)
if(OpenMP_C_FOUND)
    target_link_libraries(tpbk_stream PRIVATE OpenMP::OpenMP_C)
endif()

# Staxpy kernel as shared library
add_library(tpbk_staxpy SHARED src/kernels/simple/tpbk_staxpy.c)
target_include_directories(tpbk_staxpy PRIVATE src src/include)
target_link_libraries(tpbk_staxpy PRIVATE tpbench m)
if(OpenMP_C_FOUND)
    target_link_libraries(tpbk_staxpy PRIVATE OpenMP::OpenMP_C)
endif()

# Striad kernel as shared library
add_library(tpbk_striad SHARED src/kernels/simple/tpbk_striad.c)
target_include_directories(tpbk_striad PRIVATE src src/include)
target_link_libraries(tpbk_striad PRIVATE tpbench m)
if(OpenMP_C_FOUND)
    target_link_libraries(tpbk_striad PRIVATE OpenMP::OpenMP_C)
endif()

# Stream MPI kernel (only built when MPI is enabled)
if(MPI_C_FOUND)
    add_library(tpbk_stream_mpi SHARED src/kernels/simple/tpbk_stream_mpi.c)
    target_include_directories(tpbk_stream_mpi PRIVATE src src/include)
    target_link_libraries(tpbk_stream_mpi PRIVATE tpbench m MPI::MPI_C)
    if(OpenMP_C_FOUND)
        target_link_libraries(tpbk_stream_mpi PRIVATE OpenMP::OpenMP_C)
    endif()
endif()

# Main CLI executable and subcommands
add_executable(tpbcli
    src/tpbcli.c
    src/tpbcli-run.c
    src/tpbcli-list.c
    src/tpbcli-benchmark.c
    src/tpbcli-help.c
)
target_include_directories(tpbcli PRIVATE src src/include)
target_link_libraries(tpbcli PRIVATE tpbench tpbk_triad tpbk_stream tpbk_staxpy tpbk_striad m)

# PLI Executables (.tpbx) for triad and stream kernels
add_executable(tpbk_triad.tpbx src/kernels/simple/tpbk_triad_main.c)
target_include_directories(tpbk_triad.tpbx PRIVATE src src/include)
target_link_libraries(tpbk_triad.tpbx PRIVATE tpbench tpbk_triad m)
if(OpenMP_C_FOUND)
    target_link_libraries(tpbk_triad.tpbx PRIVATE OpenMP::OpenMP_C)
endif()

add_executable(tpbk_stream.tpbx src/kernels/simple/tpbk_stream_main.c)
target_include_directories(tpbk_stream.tpbx PRIVATE src src/include)
target_link_libraries(tpbk_stream.tpbx PRIVATE tpbench tpbk_stream m)
if(OpenMP_C_FOUND)
    target_link_libraries(tpbk_stream.tpbx PRIVATE OpenMP::OpenMP_C)
endif()

add_executable(tpbk_staxpy.tpbx src/kernels/simple/tpbk_staxpy_main.c)
target_include_directories(tpbk_staxpy.tpbx PRIVATE src src/include)
target_link_libraries(tpbk_staxpy.tpbx PRIVATE tpbench tpbk_staxpy m)
if(OpenMP_C_FOUND)
    target_link_libraries(tpbk_staxpy.tpbx PRIVATE OpenMP::OpenMP_C)
endif()

add_executable(tpbk_striad.tpbx src/kernels/simple/tpbk_striad_main.c)
target_include_directories(tpbk_striad.tpbx PRIVATE src src/include)
target_link_libraries(tpbk_striad.tpbx PRIVATE tpbench tpbk_striad m)
if(OpenMP_C_FOUND)
    target_link_libraries(tpbk_striad.tpbx PRIVATE OpenMP::OpenMP_C)
endif()

# Stream MPI PLI executable (only built when MPI is enabled)
if(MPI_C_FOUND)
    add_executable(tpbk_stream_mpi.tpbx src/kernels/simple/tpbk_stream_mpi_main.c)
    target_include_directories(tpbk_stream_mpi.tpbx PRIVATE src src/include)
    target_link_libraries(tpbk_stream_mpi.tpbx PRIVATE tpbench tpbk_stream_mpi m MPI::MPI_C)
    if(OpenMP_C_FOUND)
        target_link_libraries(tpbk_stream_mpi.tpbx PRIVATE OpenMP::OpenMP_C)
    endif()
endif()

# Install targets
install(TARGETS tpbench tpbk_triad tpbk_stream tpbk_staxpy tpbk_striad
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(TARGETS tpbcli tpbk_triad.tpbx tpbk_stream.tpbx tpbk_staxpy.tpbx tpbk_striad.tpbx
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# Conditional install for MPI targets
if(MPI_C_FOUND)
    install(TARGETS tpbk_stream_mpi
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(TARGETS tpbk_stream_mpi.tpbx
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

# Install public headers
install(DIRECTORY src/include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Copy YAML configuration files to build/etc directory
add_custom_target(copy_yaml_files ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/etc
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/setup/yaml ${CMAKE_BINARY_DIR}/etc
    COMMENT "Copying YAML configuration files to build/etc"
)
