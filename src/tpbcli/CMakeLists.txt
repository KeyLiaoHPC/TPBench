set(TPBCLI_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/src/include
    ${CMAKE_SOURCE_DIR}/src/tpbcli/main
    ${CMAKE_SOURCE_DIR}/src/tpbcli/argp
    ${CMAKE_SOURCE_DIR}/src/tpbcli/run
    ${CMAKE_SOURCE_DIR}/src/tpbcli/benchmark
    ${CMAKE_SOURCE_DIR}/src/tpbcli/database
    ${CMAKE_SOURCE_DIR}/src/tpbcli/rtenv
    ${CMAKE_SOURCE_DIR}/src/tpbcli/kernel
    ${CMAKE_SOURCE_DIR}/src/tpbcli/help
)

# CLI must exist before kernel post-build hooks reference tpbcli.
add_executable(tpbcli
    main/tpbcli.c
    argp/tpbcli-argp.c
    run/tpbcli-run.c
    run/tpbcli-run-dim.c
    kernel/tpbcli-kernel-list.c
    kernel/tpbcli-kernel-registry.c
    kernel/tpbcli-kernel-set.c
    kernel/tpbcli-kernel-get.c
    kernel/tpbcli-print-kernel-help.c
    kernel/tpbcli-kernel-backup.c
    kernel/tpbcli-kernel-init.c
    kernel/tpbcli-kernel-build.c
    kernel/tpbcli-kernel-home.c
    kernel/tpbcli-kernel.c
    benchmark/tpbcli-benchmark.c
    benchmark/tpb-bench-yaml.c
    benchmark/tpb-bench-score.c
    help/tpbcli-help.c
    help/tpbcli-help-doc.c
    database/tpbcli-database.c
    database/tpbcli-database-ls.c
    database/tpbcli-database-dump.c
    database/tpbcli-database-dump-fmt.c
    rtenv/tpbcli-rtenv.c
    rtenv/tpbcli-rtenv-new.c
    rtenv/tpbcli-rtenv-list.c
    rtenv/tpbcli-rtenv-show.c
    rtenv/tpbcli-rtenv-load.c
    rtenv/tpbcli-rtenv-template.c
)
target_include_directories(tpbcli PRIVATE ${TPBCLI_INCLUDE_DIRS})
target_compile_options(tpbcli PRIVATE -O2)
target_link_libraries(tpbcli PRIVATE tpbench m)
tpb_set_install_rpath_tpbench_exe(tpbcli)

option(TPB_CREATE_BASE_RTENV "Record build-time RTEnv snapshot after tpbcli build" ON)
if(TPB_CREATE_BASE_RTENV)
    add_custom_command(TARGET tpbcli POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DTPB_CREATE_BASE_RTENV=${TPB_CREATE_BASE_RTENV}
            -DTPBCLI=$<TARGET_FILE:tpbcli>
            -DTPB_HOME=${CMAKE_BINARY_DIR}
            -P ${CMAKE_SOURCE_DIR}/cmake/TPBenchBaseRtenvRun.cmake
        COMMENT "Record build-time runtime environment snapshot in build workspace"
        VERBATIM)
endif()

add_executable(tpbcli-pli-launcher pli/tpbcli-pli-launcher.c)
target_include_directories(tpbcli-pli-launcher PRIVATE
    ${CMAKE_BINARY_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/include)
target_link_libraries(tpbcli-pli-launcher PRIVATE tpbench dl)
target_compile_options(tpbcli-pli-launcher PRIVATE -O2)
set_target_properties(tpbcli-pli-launcher PROPERTIES OUTPUT_NAME "tpbcli-pli-launcher")
tpb_set_install_rpath_tpbench_exe(tpbcli-pli-launcher)

install(TARGETS tpbcli
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
