set(TPB_CORELIB_SOURCES
    tpb-driver.c
    tpb-io.c
    tpb-stat.c
    tpb-unitcast.c
    tpb-impl.c
    tpb-argp.c
    tpb-dynloader.c
)

add_library(tpbench SHARED ${TPB_CORELIB_SOURCES} ${TPB_TIMER_SOURCES})
target_include_directories(tpbench
    PUBLIC  ${CMAKE_SOURCE_DIR}/src/include
            ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(tpbench PUBLIC m dl)
target_compile_definitions(tpbench PRIVATE TPB_DIR="${TPB_DIR_RESOLVED}")
