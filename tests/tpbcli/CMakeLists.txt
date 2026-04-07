# --- run argv: --kernel required before k* options (Pack B2) ---
add_executable(test-cli-run-nokernel EXCLUDE_FROM_ALL
    test-cli-run-nokernel.c
)
target_compile_definitions(test-cli-run-nokernel PRIVATE
    "TPB_TEST_TPBCLI_STR=\"$<TARGET_FILE:tpbcli>\""
)
add_dependencies(test-cli-run-nokernel tpbcli)
set_target_properties(test-cli-run-nokernel PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${TPB_TEST_RUNTIME_OUTPUT_DIRECTORY}"
    LIBRARY_OUTPUT_DIRECTORY "${TPB_TEST_LIBRARY_OUTPUT_DIRECTORY}"
    ARCHIVE_OUTPUT_DIRECTORY "${TPB_TEST_ARCHIVE_OUTPUT_DIRECTORY}"
)

tpb_add_subcase(B2 B2.1 kargs_before_kernel       test-cli-run-nokernel tpbcli)
tpb_add_subcase(B2 B2.2 kargs_dim_before_kernel   test-cli-run-nokernel tpbcli)
tpb_add_subcase(B2 B2.3 kenvs_before_kernel       test-cli-run-nokernel tpbcli)
tpb_add_subcase(B2 B2.4 kmpiargs_before_kernel    test-cli-run-nokernel tpbcli)
# B2.5 runs --kernel stream; omit when TPB_KERNELS excludes stream (see integration tests).
if(TARGET tpbk_stream.tpbx)
    tpb_add_subcase(B2 B2.5 normal_with_kernel test-cli-run-nokernel tpbcli)
endif()
tpb_add_subcase(B2 B2.6 kenvs_dim_before_kernel   test-cli-run-nokernel tpbcli)
tpb_add_subcase(B2 B2.7 kmpiargs_dim_before_kernel test-cli-run-nokernel tpbcli)

if(TARGET tpbk_stream.tpbx)
    tpb_add_subcase(B2 B2.8 dry_run_basic test-cli-run-nokernel tpbcli)
endif()
tpb_add_subcase(B2 B2.9 help_run_level test-cli-run-nokernel tpbcli)
tpb_add_subcase(B2 B2.10 help_kernel_no_name test-cli-run-nokernel tpbcli)
tpb_add_subcase(B2 B2.11 bad_kernel_name test-cli-run-nokernel tpbcli)
if(TARGET tpbk_stream.tpbx)
    tpb_add_subcase(B2 B2.12 dry_run_cartesian test-cli-run-nokernel tpbcli)
    tpb_add_subcase(B2 B2.13 help_kernel_specific test-cli-run-nokernel tpbcli)
endif()

# --- dimargs test executable (Pack B1) ---
add_executable(test-cli-run-dimargs EXCLUDE_FROM_ALL
    test-cli-run-dimargs.c
    ${CMAKE_SOURCE_DIR}/src/tpbcli-run-dim.c
)
target_include_directories(test-cli-run-dimargs PRIVATE ${CMAKE_SOURCE_DIR}/src)
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
    ${CMAKE_SOURCE_DIR}/src/tpbcli-argp.c
)
target_include_directories(test-tpbcli-argp PRIVATE ${CMAKE_SOURCE_DIR}/src)
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

# Aggregate targets
add_custom_target(test_tpbcli
    DEPENDS test-cli-run-dimargs test-cli-run-nokernel test-tpbcli-argp
)

add_custom_target(check_tpbcli
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -L "^tpbcli$"
    DEPENDS test_tpbcli
    USES_TERMINAL
)
