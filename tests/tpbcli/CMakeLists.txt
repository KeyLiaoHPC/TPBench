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
    "TPB_TEST_WORKSPACE=\"${CMAKE_BINARY_DIR}\""
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
tpb_add_subcase(B4 B4.10 database_list_domain_task test-cli-database tpbcli)
tpb_add_subcase(B4 B4.11 database_list_domain_kernel test-cli-database tpbcli)
tpb_add_subcase(B4 B4.12 database_list_count_conflict test-cli-database tpbcli)
tpb_add_subcase(B4 B4.13 database_list_domain_conflict test-cli-database tpbcli)
tpb_add_subcase(B4 B4.14 database_list_domain_invalid test-cli-database tpbcli)
tpb_add_subcase(B4 B4.15 database_list_domain_rtenv test-cli-database tpbcli)
tpb_add_subcase(B4 B4.16 database_list_domain_conflict_dr test-cli-database tpbcli)
tpb_add_subcase(B4 B4.17 database_dump_domain_id_prefix test-cli-database tpbcli)
tpb_add_subcase(B4 B4.18 database_dump_missing_domain test-cli-database tpbcli)
tpb_add_subcase(B4 B4.19 database_dump_missing_mode test-cli-database tpbcli)
tpb_add_subcase(B4 B4.20 database_dump_id_entry_conflict test-cli-database tpbcli)
tpb_add_subcase(B4 B4.21 database_dump_domain_conflict test-cli-database tpbcli)
tpb_add_subcase(B4 B4.22 database_dump_rtenv_entry test-cli-database tpbcli)
tpb_add_subcase(B4 B4.23 database_dump_rtenv_record test-cli-database tpbcli)
tpb_add_subcase(B4 B4.24 database_dump_count_conflict test-cli-database tpbcli)

# --- rtenv subcommand (Pack B6) ---
add_executable(test-cli-rtenv EXCLUDE_FROM_ALL
    test-cli-rtenv.c
)
target_compile_definitions(test-cli-rtenv PRIVATE
    "TPB_TEST_TPBCLI_STR=\"$<TARGET_FILE:tpbcli>\""
)
target_include_directories(test-cli-rtenv PRIVATE
    ${CMAKE_SOURCE_DIR}/src/include
)
target_link_libraries(test-cli-rtenv PRIVATE tpbench m)
add_dependencies(test-cli-rtenv tpbcli)
set_target_properties(test-cli-rtenv PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${TPB_TEST_RUNTIME_OUTPUT_DIRECTORY}"
    LIBRARY_OUTPUT_DIRECTORY "${TPB_TEST_LIBRARY_OUTPUT_DIRECTORY}"
    ARCHIVE_OUTPUT_DIRECTORY "${TPB_TEST_ARCHIVE_OUTPUT_DIRECTORY}"
)

tpb_add_subcase(B6 B6.1  rtenv_missing_subcmd     test-cli-rtenv tpbcli)
tpb_add_subcase(B6 B6.2  rtenv_help               test-cli-rtenv tpbcli)
tpb_add_subcase(B6 B6.3  new_output_template      test-cli-rtenv tpbcli)
tpb_add_subcase(B6 B6.4  new_from_file            test-cli-rtenv tpbcli)
tpb_add_subcase(B6 B6.5  new_inline               test-cli-rtenv tpbcli)
tpb_add_subcase(B6 B6.6  new_bad_template         test-cli-rtenv tpbcli)
tpb_add_subcase(B6 B6.7  list_activated           test-cli-rtenv tpbcli)
tpb_add_subcase(B6 B6.8  show_tables              test-cli-rtenv tpbcli)
tpb_add_subcase(B6 B6.9  load_shell_fragment      test-cli-rtenv tpbcli)
tpb_add_subcase(B6 B6.10 new_requires_active      test-cli-rtenv tpbcli)

# --- task ls subcommand (Pack B7) ---
add_executable(test-cli-task-ls EXCLUDE_FROM_ALL
    test-cli-task-ls.c
    test-cli-task-fixture.c
)
target_compile_definitions(test-cli-task-ls PRIVATE
    "TPB_TEST_TPBCLI_STR=\"$<TARGET_FILE:tpbcli>\""
)
target_include_directories(test-cli-task-ls PRIVATE
    ${CMAKE_SOURCE_DIR}/src/include
)
target_link_libraries(test-cli-task-ls PRIVATE tpbench m)
add_dependencies(test-cli-task-ls tpbcli)
set_target_properties(test-cli-task-ls PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${TPB_TEST_RUNTIME_OUTPUT_DIRECTORY}"
    LIBRARY_OUTPUT_DIRECTORY "${TPB_TEST_LIBRARY_OUTPUT_DIRECTORY}"
    ARCHIVE_OUTPUT_DIRECTORY "${TPB_TEST_ARCHIVE_OUTPUT_DIRECTORY}"
)

tpb_add_subcase(B7 B7.1  task_missing_subcmd      test-cli-task-ls tpbcli)
tpb_add_subcase(B7 B7.2  task_help_overview       test-cli-task-ls tpbcli)
tpb_add_subcase(B7 B7.3  task_ls_help             test-cli-task-ls tpbcli)
tpb_add_subcase(B7 B7.4  task_ls_alias            test-cli-task-ls tpbcli)
tpb_add_subcase(B7 B7.5  ls_empty_workspace       test-cli-task-ls tpbcli)
tpb_add_subcase(B7 B7.6  ls_entrypoint_only       test-cli-task-ls tpbcli)
tpb_add_subcase(B7 B7.7  ls_count_limits          test-cli-task-ls tpbcli)
tpb_add_subcase(B7 B7.8  ls_exit_code_filter      test-cli-task-ls tpbcli)
tpb_add_subcase(B7 B7.9  ls_reject_bad_filters    test-cli-task-ls tpbcli)
tpb_add_subcase(B7 B7.10 ls_local_time_header     test-cli-task-ls tpbcli)
tpb_add_subcase(B7 B7.11 ls_capsule_subproc       test-cli-task-ls tpbcli)
tpb_add_subcase(B7 B7.12 ls_ridmap_update         test-cli-task-ls tpbcli)
tpb_add_subcase(B7 B7.13 ls_ridmap_zero_preserve  test-cli-task-ls tpbcli)
tpb_add_subcase(B7 B7.14 ls_count_conflict        test-cli-task-ls tpbcli)
tpb_add_subcase(B7 B7.15 ls_kernel_id_filter      test-cli-task-ls tpbcli)

add_executable(test-cli-task-get-result EXCLUDE_FROM_ALL
    test-cli-task-get-result.c
    test-cli-task-fixture.c
    ${CMAKE_SOURCE_DIR}/src/tpbcli/task/tpbcli-task-select.c
    ${CMAKE_SOURCE_DIR}/src/tpbcli/task/tpbcli-task-get-result.c
    ${CMAKE_SOURCE_DIR}/src/tpbcli/task/tpbcli-task-time.c
    ${CMAKE_SOURCE_DIR}/src/tpbcli/common/tpbcli-data-fmt.c
)
target_compile_definitions(test-cli-task-get-result PRIVATE
    "TPB_TEST_TPBCLI_STR=\"$<TARGET_FILE:tpbcli>\""
)
target_include_directories(test-cli-task-get-result PRIVATE
    ${CMAKE_SOURCE_DIR}/src/include
    ${CMAKE_SOURCE_DIR}/src/tpbcli/task
    ${CMAKE_SOURCE_DIR}/src/tpbcli/common
)
target_link_libraries(test-cli-task-get-result PRIVATE tpbench m)
add_dependencies(test-cli-task-get-result tpbcli)
set_target_properties(test-cli-task-get-result PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${TPB_TEST_RUNTIME_OUTPUT_DIRECTORY}"
    LIBRARY_OUTPUT_DIRECTORY "${TPB_TEST_LIBRARY_OUTPUT_DIRECTORY}"
    ARCHIVE_OUTPUT_DIRECTORY "${TPB_TEST_ARCHIVE_OUTPUT_DIRECTORY}"
)

tpb_add_subcase(B7 B7.20 name_csv              test-cli-task-get-result tpbcli)
tpb_add_subcase(B7 B7.21 id_prefix_resolve     test-cli-task-get-result tpbcli)
tpb_add_subcase(B7 B7.22 pooled_samples        test-cli-task-get-result tpbcli)
tpb_add_subcase(B7 B7.23 metric_missing        test-cli-task-get-result tpbcli)
tpb_add_subcase(B7 B7.24 follow_derive_member  test-cli-task-get-result tpbcli)
tpb_add_subcase(B7 B7.25 gr_alias               test-cli-task-get-result tpbcli)
tpb_add_subcase(B7 B7.26 meta_only_mode          test-cli-task-get-result tpbcli)
tpb_add_subcase(B7 B7.27 data_only_mode          test-cli-task-get-result tpbcli)
tpb_add_subcase(B7 B7.28 data_name_context_help  test-cli-task-get-result tpbcli)
tpb_add_subcase(B7 B7.29 show_each_subrank       test-cli-task-get-result tpbcli)
tpb_add_subcase(B7 B7.30 duplicate_header_warn   test-cli-task-get-result tpbcli)
tpb_add_subcase(B7 B7.31 all_members_fail_warn   test-cli-task-get-result tpbcli)
tpb_add_subcase(B7 B7.32 unit_mismatch           test-cli-task-get-result tpbcli)
tpb_add_subcase(B7 B7.33 shape_mismatch          test-cli-task-get-result tpbcli)
tpb_add_subcase(B7 B7.34 partial_members_used    test-cli-task-get-result tpbcli)
tpb_add_subcase(B7 B7.35 batch_host_na_warn      test-cli-task-get-result tpbcli)
tpb_add_subcase(B7 B7.36 meta_scalar_keys        test-cli-task-get-result tpbcli)
tpb_add_subcase(B7 B7.37 header_field_meta       test-cli-task-get-result tpbcli)
tpb_add_subcase(B7 B7.38 name_report_meta        test-cli-task-get-result tpbcli)

add_executable(test-cli-task-export EXCLUDE_FROM_ALL
    test-cli-task-export.c
    test-cli-task-fixture.c
    ${CMAKE_SOURCE_DIR}/src/tpbcli/task/tpbcli-task-select.c
    ${CMAKE_SOURCE_DIR}/src/tpbcli/task/tpbcli-task-csv.c
    ${CMAKE_SOURCE_DIR}/src/tpbcli/task/tpbcli-task-time.c
)
target_compile_definitions(test-cli-task-export PRIVATE
    "TPB_TEST_TPBCLI_STR=\"$<TARGET_FILE:tpbcli>\""
)
target_include_directories(test-cli-task-export PRIVATE
    ${CMAKE_SOURCE_DIR}/src/include
    ${CMAKE_SOURCE_DIR}/src/tpbcli/task
)
target_link_libraries(test-cli-task-export PRIVATE tpbench m)
add_dependencies(test-cli-task-export tpbcli)
set_target_properties(test-cli-task-export PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${TPB_TEST_RUNTIME_OUTPUT_DIRECTORY}"
    LIBRARY_OUTPUT_DIRECTORY "${TPB_TEST_LIBRARY_OUTPUT_DIRECTORY}"
    ARCHIVE_OUTPUT_DIRECTORY "${TPB_TEST_ARCHIVE_OUTPUT_DIRECTORY}"
)

tpb_add_subcase(B7 B7.40 export_standalone_pair   test-cli-task-export tpbcli)
tpb_add_subcase(B7 B7.41 csv_double_comma         test-cli-task-export tpbcli)
tpb_add_subcase(B7 B7.42 export_from_ls           test-cli-task-export tpbcli)
tpb_add_subcase(B7 B7.43 export_capsule_members  test-cli-task-export tpbcli)
tpb_add_subcase(B7 B7.44 export_subrank_filter   test-cli-task-export tpbcli)
tpb_add_subcase(B7 B7.45 export_non_tty_default  test-cli-task-export tpbcli)
tpb_add_subcase(B7 B7.46 export_invalid_id       test-cli-task-export tpbcli)
tpb_add_subcase(B7 B7.47 export_filter_parse      test-cli-task-export tpbcli)
tpb_add_subcase(B7 B7.48 export_id_prefix        test-cli-task-export tpbcli)
tpb_add_subcase(B7 B7.49 export_env_two_columns  test-cli-task-export tpbcli)

# --- kernel subcommand (Pack B5) ---
add_executable(test-cli-kernel EXCLUDE_FROM_ALL
    test-cli-kernel.c
)
target_include_directories(test-cli-kernel PRIVATE ${CMAKE_SOURCE_DIR}/src)
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
            test-cli-database test-cli-rtenv test-cli-kernel test-cli-task-ls
            test-cli-task-get-result test-cli-task-export
)

add_custom_target(check_tpbcli
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -L "^tpbcli$"
    DEPENDS test_tpbcli
    USES_TERMINAL
)
