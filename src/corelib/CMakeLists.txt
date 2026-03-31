set(TPB_CORELIB_SOURCES
    tpb-driver.c
    tpb_corelib_state.c
    tpb-io.c
    tpb-stat.c
    tpb-unitcast.c
    tpb-impl.c
    tpb-argp.c
    tpb-dynloader.c
    tpb-autorecord.c
    strftime.c
    rafdb/tpb-raf-workspace.c
    rafdb/tpb-raf-magic.c
    rafdb/tpb-raf-entry.c
    rafdb/tpb-raf-record.c
    rafdb/tpb-raf-id.c
    rafdb/tpb-raf-merge.c
    rafdb/tpb-sha1.c
)

add_library(tpbench SHARED ${TPB_CORELIB_SOURCES} ${TPB_TIMER_SOURCES})
target_compile_options(tpbench PRIVATE -O2)
target_include_directories(tpbench
    PUBLIC  ${CMAKE_SOURCE_DIR}/src/include
            ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(tpbench PUBLIC m dl rt)
target_compile_definitions(tpbench PRIVATE TPB_DIR="${TPB_DIR_RESOLVED}")
