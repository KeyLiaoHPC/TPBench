# --- run argv: --kernel required before k* options (Pack B2) ---
add_executable(test-cli-run-nokernel EXCLUDE_FROM_ALL
    test-cli-run-nokernel.c
)
target_compile_definitions(test-cli-run-nokernel PRIVATE
    "TPB_TEST_TPBCLI_STR=\"$<TARGET_FILE:tpbcli>\""
    "TPB_TEST_TPB_HOME=\"${CMAKE_BINARY_DIR}\""
)
add_dependencies(test-cli-run-nokernel tpbcli)
if(TARGET tpbk_stream)
    add_dependencies(test-cli-run-nokernel tpbk_stream)
endif()
set_target_properties(test-cli-run-nokernel PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${TPB_TEST_RUNTIME_OUTPUT_DIRECTORY}"
    LIBRARY_OUTPUT_DIRECTORY "${TPB_TEST_LIBRARY_OUTPUT_DIRECTORY}"
    ARCHIVE_OUTPUT_DIRECTORY "${TPB_TEST_ARCHIVE_OUTPUT_DIRECTORY}"
)

tpb_add_subcase(B2 B2.1 kargs_before_kernel       test-cli-run-nokernel tpbcli)
tpb_add_subcase(B2 B2.2 kargs_dim_before_kernel   test-cli-run-nokernel tpbcli)
tpb_add_subcase(B2 B2.3 kenvs_before_kernel       test-cli-run-nokernel tpbcli)
tpb_add_subcase(B2 B2.4 wrapper_args_without_wrapper test-cli-run-nokernel tpbcli)
# B2.5 runs --kernel stream; omit when TPB_KERNELS excludes stream (see integration tests).
if(TARGET tpbk_stream)
    tpb_add_subcase(B2 B2.5 normal_with_kernel test-cli-run-nokernel tpbcli)
endif()
tpb_add_subcase(B2 B2.6 kenvs_dim_before_kernel   test-cli-run-nokernel tpbcli)

if(TARGET tpbk_stream)
    tpb_add_subcase(B2 B2.8 dry_run_basic test-cli-run-nokernel tpbcli)
endif()
tpb_add_subcase(B2 B2.9 help_run_level test-cli-run-nokernel tpbcli)
tpb_add_subcase(B2 B2.10 help_kernel_no_name test-cli-run-nokernel tpbcli)
tpb_add_subcase(B2 B2.11 bad_kernel_name test-cli-run-nokernel tpbcli)
if(TARGET tpbk_stream)
    tpb_add_subcase(B2 B2.12 dry_run_cartesian test-cli-run-nokernel tpbcli)
    tpb_add_subcase(B2 B2.13 help_kernel_specific test-cli-run-nokernel tpbcli)
    tpb_add_subcase(B2 B2.14 run_kernel_found_id test-cli-run-nokernel tpbcli)
    tpb_add_subcase(B2 B2.15 run_begin_batch_fail test-cli-run-nokernel tpbcli)
endif()

# --- wrapper dry-run command generation (Pack W1) ---
add_executable(test-cli-run-wrapper EXCLUDE_FROM_ALL
    test-cli-run-wrapper.c
)
target_compile_definitions(test-cli-run-wrapper PRIVATE
    "TPB_TEST_TPBCLI_STR=\"$<TARGET_FILE:tpbcli>\""
    "TPB_TEST_TPB_HOME=\"${CMAKE_BINARY_DIR}\""
)
add_dependencies(test-cli-run-wrapper tpbcli)
if(TARGET tpbk_stream)
    add_dependencies(test-cli-run-wrapper tpbk_stream)
    tpb_add_subcase(W1 W1.1 no_wrapper              test-cli-run-wrapper tpbcli)
    tpb_add_subcase(W1 W1.2 global_wrapper_only     test-cli-run-wrapper tpbcli)
    tpb_add_subcase(W1 W1.3 global_local_chain      test-cli-run-wrapper tpbcli)
    tpb_add_subcase(W1 W1.4 override_global         test-cli-run-wrapper tpbcli)
    tpb_add_subcase(W1 W1.5 multiple_kernels        test-cli-run-wrapper tpbcli)
endif()
tpb_add_subcase(W1 W1.6 wrapper_args_without_wrapper test-cli-run-wrapper tpbcli)
set_target_properties(test-cli-run-wrapper PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${TPB_TEST_RUNTIME_OUTPUT_DIRECTORY}"
    LIBRARY_OUTPUT_DIRECTORY "${TPB_TEST_LIBRARY_OUTPUT_DIRECTORY}"
    ARCHIVE_OUTPUT_DIRECTORY "${TPB_TEST_ARCHIVE_OUTPUT_DIRECTORY}"
)

# --- dimargs test executable (Pack B1) ---
add_executable(test-cli-run-dimargs EXCLUDE_FROM_ALL
    test-cli-run-dimargs.c
    ${CMAKE_SOURCE_DIR}/src/tpbcli/run/tpbcli-run-dim.c
)
target_include_directories(test-cli-run-dimargs PRIVATE
    ${CMAKE_SOURCE_DIR}/src/include
    ${CMAKE_SOURCE_DIR}/src/tpbcli/run
)
target_link_libraries(test-cli-run-dimargs PRIVATE tpbench m)
set_target_properties(test-cli-run-dimargs PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${TPB_TEST_RUNTIME_OUTPUT_DIRECTORY}"
    LIBRARY_OUTPUT_DIRECTORY "${TPB_TEST_LIBRARY_OUTPUT_DIRECTORY}"
    ARCHIVE_OUTPUT_DIRECTORY "${TPB_TEST_ARCHIVE_OUTPUT_DIRECTORY}"
)

# Pack B1 sub-cases
tpb_add_subcase(B1 B1.1 list_expansion   test-cli-run-dimargs tpbcli)
tpb_add_subcase(B1 B1.2 recur_expansion  test-cli-run-dimargs tpbcli)
tpb_add_subcase(B1 B1.3 string_list      test-cli-run-dimargs tpbcli)
tpb_add_subcase(B1 B1.4 nested_rejected test-cli-run-dimargs tpbcli)
tpb_add_subcase(B1 B1.5 total_count      test-cli-run-dimargs tpbcli)

# --- tpbcli-argp (Pack B3) ---
add_executable(test-tpbcli-argp EXCLUDE_FROM_ALL
    test_tpbcli_argp.c
    ${CMAKE_SOURCE_DIR}/src/tpbcli/argp/tpbcli-argp.c
)
target_include_directories(test-tpbcli-argp PRIVATE
    ${CMAKE_SOURCE_DIR}/src/include
    ${CMAKE_SOURCE_DIR}/src/tpbcli/argp
)
target_link_libraries(test-tpbcli-argp PRIVATE tpbench m)
set_target_properties(test-tpbcli-argp PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${TPB_TEST_RUNTIME_OUTPUT_DIRECTORY}"
    LIBRARY_OUTPUT_DIRECTORY "${TPB_TEST_LIBRARY_OUTPUT_DIRECTORY}"
    ARCHIVE_OUTPUT_DIRECTORY "${TPB_TEST_ARCHIVE_OUTPUT_DIRECTORY}"
)

tpb_add_subcase(B3 B3.1  tree_lifecycle       test-tpbcli-argp tpbcli)
tpb_add_subcase(B3 B3.2  parse_basic          test-tpbcli-argp tpbcli)
tpb_add_subcase(B3 B3.3  mandatory_and_preset test-tpbcli-argp tpbcli)
tpb_add_subcase(B3 B3.4  exclusive_conflict   test-tpbcli-argp tpbcli)
tpb_add_subcase(B3 B3.5  conflict_opts        test-tpbcli-argp tpbcli)
tpb_add_subcase(B3 B3.6  help_dispatch        test-tpbcli-argp tpbcli)
tpb_add_subcase(B3 B3.7  unknown_arg          test-tpbcli-argp tpbcli)
tpb_add_subcase(B3 B3.8  stack_pop_retry      test-tpbcli-argp tpbcli)
tpb_add_subcase(B3 B3.9  max_chosen           test-tpbcli-argp tpbcli)
tpb_add_subcase(B3 B3.10 find_arg             test-tpbcli-argp tpbcli)
tpb_add_subcase(B3 B3.11 delegate_subcmd      test-tpbcli-argp tpbcli)

# --- database subcommand (Pack B4) ---
add_executable(test-cli-database EXCLUDE_FROM_ALL
    test-cli-database.c
)
target_compile_definitions(test-cli-database PRIVATE
    "TPB_TEST_TPBCLI_STR=\"$<TARGET_FILE:tpbcli>\""
)
add_dependencies(test-cli-database tpbcli)
set_target_properties(test-cli-database PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${TPB_TEST_RUNTIME_OUTPUT_DIRECTORY}"
    LIBRARY_OUTPUT_DIRECTORY "${TPB_TEST_LIBRARY_OUTPUT_DIRECTORY}"
    ARCHIVE_OUTPUT_DIRECTORY "${TPB_TEST_ARCHIVE_OUTPUT_DIRECTORY}"
)

tpb_add_subcase(B4 B4.1  database_missing_subcmd   test-cli-database tpbcli)
tpb_add_subcase(B4 B4.2  database_help_overview    test-cli-database tpbcli)
tpb_add_subcase(B4 B4.3  database_list_help        test-cli-database tpbcli)
tpb_add_subcase(B4 B4.4  database_dump_help        test-cli-database tpbcli)
tpb_add_subcase(B4 B4.5  database_dump_no_selector test-cli-database tpbcli)
tpb_add_subcase(B4 B4.6  database_dump_conflict    test-cli-database tpbcli)
tpb_add_subcase(B4 B4.7  database_unknown_action   test-cli-database tpbcli)
tpb_add_subcase(B4 B4.8  database_dump_unknown_flag test-cli-database tpbcli)
tpb_add_subcase(B4 B4.9  database_ls_alias         test-cli-database tpbcli)

# --- kernel subcommand (Pack B5) ---
add_executable(test-cli-kernel EXCLUDE_FROM_ALL
    test-cli-kernel.c
)
target_compile_definitions(test-cli-kernel PRIVATE
    "TPB_TEST_TPBCLI_STR=\"$<TARGET_FILE:tpbcli>\""
)
add_dependencies(test-cli-kernel tpbcli)
set_target_properties(test-cli-kernel PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${TPB_TEST_RUNTIME_OUTPUT_DIRECTORY}"
    LIBRARY_OUTPUT_DIRECTORY "${TPB_TEST_LIBRARY_OUTPUT_DIRECTORY}"
    ARCHIVE_OUTPUT_DIRECTORY "${TPB_TEST_ARCHIVE_OUTPUT_DIRECTORY}"
)

tpb_add_subcase(B5 B5.1 set_missing_args test-cli-kernel tpbcli)
set(_tpb_kernel_test_ws "${CMAKE_BINARY_DIR}/tests/kernel_hist_ws")
set(_tpb_kernel_init_dir "${CMAKE_BINARY_DIR}/tests/kinit_b5")
target_compile_definitions(test-cli-kernel PRIVATE
    "TPB_TEST_TPB_HOME=\"${CMAKE_BINARY_DIR}\""
    "TPB_TEST_KERNEL_WORKSPACE=\"${_tpb_kernel_test_ws}\""
    "TPB_TEST_KERNEL_INIT_DIR=\"${_tpb_kernel_init_dir}\"")
if(TARGET tpbk_stream)
    add_dependencies(test-cli-kernel tpbk_stream)
    tpb_add_subcase(B5 B5.2 get_readonly test-cli-kernel tpbcli)
    tpb_add_subcase(B5 B5.3 set_get_metadata test-cli-kernel tpbcli)
endif()
add_dependencies(test-cli-kernel copy_cmake_package copy_yaml_files)
tpb_add_subcase(B5 B5.4 init_missing_args test-cli-kernel tpbcli)
tpb_add_subcase(B5 B5.5 init_build_template test-cli-kernel tpbcli)
tpb_add_subcase(B5 B5.6 build_missing_args test-cli-kernel tpbcli)
tpb_add_subcase(B5 B5.7 build_exclusive_selectors test-cli-kernel tpbcli)
tpb_add_subcase(B5 B5.8 list_tags_column test-cli-kernel tpbcli)
tpb_add_subcase(B5 B5.9 build_unknown_tag test-cli-kernel tpbcli)

# Aggregate targets
add_custom_target(test_tpbcli
    DEPENDS test-cli-run-dimargs test-cli-run-nokernel test-tpbcli-argp
            test-cli-database test-cli-kernel
)

add_custom_target(check_tpbcli
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -L "^tpbcli$"
    DEPENDS test_tpbcli
    USES_TERMINAL
)
