cmake_minimum_required(VERSION 3.16) # Require at least CMake 3.16
project(TPBench C) # Project name and language

# Use C11 and require it
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Set default optimization to -O2
add_compile_options(-O2)


# Main TPBench sources
set(TPB_SOURCES
    src/tpb-driver.c
    src/tpb-io.c
    src/tpb-stat.c
    src/tpb-unitcast.c
    src/tpb-impl.c
    src/tpmpi.c
    src/tpb-cli-parse.c
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
add_library(triad SHARED src/kernels/simple/triad.c)
target_include_directories(triad PRIVATE src src/include)
target_link_libraries(triad PRIVATE tpbench m)

# Main CLI executable and subcommands
add_executable(tpbcli
    src/tpb-cli.c
    src/tpb-launch.c
    src/tpb-help.c
    src/tpb-run.c
    src/tpb-benchmark.c
)
target_include_directories(tpbcli PRIVATE src src/include)
target_link_libraries(tpbcli PRIVATE tpbench triad m)
