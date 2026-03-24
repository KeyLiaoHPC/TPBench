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
    raw_db/tpb-rawdb-workspace.c
    raw_db/tpb-rawdb-magic.c
    raw_db/tpb-rawdb-entry.c
    raw_db/tpb-rawdb-record.c
    raw_db/tpb-rawdb-id.c
    raw_db/tpb-sha1.c
)

add_library(tpbench SHARED ${TPB_CORELIB_SOURCES} ${TPB_TIMER_SOURCES})
target_include_directories(tpbench
    PUBLIC  ${CMAKE_SOURCE_DIR}/src/include
            ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(tpbench PUBLIC m dl)
target_compile_definitions(tpbench PRIVATE TPB_DIR="${TPB_DIR_RESOLVED}")
