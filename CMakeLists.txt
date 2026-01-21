cmake_minimum_required(VERSION 3.16) # Require at least CMake 3.16
project(TPBench C) # Project name and language

# Use C11 and require it
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Set default optimization to -O2
add_compile_options(-O2)

# Set output directories for build artifacts
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Set install directories
set(CMAKE_INSTALL_BINDIR bin)
set(CMAKE_INSTALL_LIBDIR lib)
set(CMAKE_INSTALL_INCLUDEDIR include)


# Main TPBench sources
set(TPB_SOURCES
    src/tpb-driver.c
    src/tpb-io.c
    src/tpb-stat.c
    src/tpb-unitcast.c
    src/tpb-impl.c
    src/tpb-argp.c
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
target_link_libraries(tpbench PUBLIC m)

# Triad kernel as shared library
add_library(triad SHARED src/kernels/simple/tpbk_triad.c)
target_include_directories(triad PRIVATE src src/include)
target_link_libraries(triad PRIVATE tpbench m)

# Stream kernel as shared library
add_library(tpbk_stream SHARED src/kernels/simple/tpbk_stream.c)
target_include_directories(tpbk_stream PRIVATE src src/include)
target_link_libraries(tpbk_stream PRIVATE tpbench m)

# Main CLI executable and subcommands
add_executable(tpbcli
    src/tpbcli.c
    src/tpbcli-run.c
    src/tpbcli-list.c
    src/tpbcli-benchmark.c
    src/tpbcli-help.c
)
target_include_directories(tpbcli PRIVATE src src/include)
target_link_libraries(tpbcli PRIVATE tpbench triad tpbk_stream m)

# Install targets
install(TARGETS tpbench triad tpbk_stream
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(TARGETS tpbcli
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# Install public headers
install(DIRECTORY src/include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
