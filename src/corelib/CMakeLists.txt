set(TPB_CORELIB_SOURCES
    tpb-driver.c
    tpb_corelib_state.c
    tpb-io.c
    tpb-stat.c
    tpb-unitcast.c
    tpb-impl.c
    tpb-argp.c
    tpb-tag-norm.c
    tpb-dynloader.c
    tpb-autorecord.c
    tpb-rtenv.c
    strftime.c
    tpblog/tpb-log.c
    tpblog/tpb-printf.c
    rafdb/rafdb-l1-domain-reg.c
    rafdb/rafdb-l1-magic.c
    rafdb/rafdb-l1-sha1.c
    rafdb/rafdb-l1-id-util.c
    rafdb/rafdb-l1-workspace.c
    rafdb/rafdb-l1-entry.c
    rafdb/rafdb-l1-record-io.c
    rafdb/rafdb-l1-record-path.c
    rafdb/rafdb-l1-record-locate.c
    rafdb/rafdb-l1-scan.c
    rafdb/rafdb-l2-tbatch.c
    rafdb/rafdb-l2-kernel.c
    rafdb/rafdb-l2-task.c
    rafdb/rafdb-l2-kernel-meta-build.c
    rafdb/rafdb-l2-runtime-environment.c
    rafdb/rafdb-l2-record-peek-magics.c
    rafdb/rafdb-l3-tbatch-taglink.c
    rafdb/rafdb-l3-runtime-environment-counter.c
    rafdb/rafdb-l3-runtime-environment-derive.c
    rafdb/rafdb-l3-task-taglink.c
    rafdb/rafdb-l3-kernel-kvmeta.c
    rafdb/rafdb-l3-kernel-patch.c
)

add_library(tpbench SHARED ${TPB_CORELIB_SOURCES} ${TPB_TIMER_SOURCES})
target_compile_options(tpbench PRIVATE -O2)
target_include_directories(tpbench
    PUBLIC  ${CMAKE_BINARY_DIR}/include
    PRIVATE ${CMAKE_SOURCE_DIR}/src/include
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/rafdb
            ${CMAKE_CURRENT_SOURCE_DIR}/tpblog
)
# libm always; libdl via CMAKE_DL_LIBS (empty on some platforms).
# librt: Linux only — provides clock_gettime on older glibc; macOS/BSD have
# no librt and reject -lrt at link time. Linux behavior unchanged.
target_link_libraries(tpbench PUBLIC m ${CMAKE_DL_LIBS})
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(tpbench PUBLIC rt)
endif()
target_compile_definitions(tpbench PRIVATE TPB_HOME="${TPB_HOME_RESOLVED}")
