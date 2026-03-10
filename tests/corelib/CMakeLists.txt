# --- FLI test executable (Pack A1) ---
add_executable(test-tpb-run-fli EXCLUDE_FROM_ALL
    tpb_run_fli.c
    mock_kernel.c
    mock_timer.c
)
target_include_directories(test-tpb-run-fli PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test-tpb-run-fli PRIVATE tpbench m)
set_target_properties(test-tpb-run-fli PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${TPB_TEST_RUNTIME_OUTPUT_DIRECTORY}"
    LIBRARY_OUTPUT_DIRECTORY "${TPB_TEST_LIBRARY_OUTPUT_DIRECTORY}"
    ARCHIVE_OUTPUT_DIRECTORY "${TPB_TEST_ARCHIVE_OUTPUT_DIRECTORY}"
)

# Pack A1 sub-cases
tpb_add_subcase(A1 A1.1 basic_success   test-tpb-run-fli corelib)
tpb_add_subcase(A1 A1.2 null_handle     test-tpb-run-fli corelib)
tpb_add_subcase(A1 A1.3 empty_runner    test-tpb-run-fli corelib)
tpb_add_subcase(A1 A1.4 kernel_warn     test-tpb-run-fli corelib)
tpb_add_subcase(A1 A1.5 kernel_fail     test-tpb-run-fli corelib)
tpb_add_subcase(A1 A1.6 param_retrieval test-tpb-run-fli corelib)

# --- PLI test executable (Pack A2) -- mock_dynloader.c interposes tpb_dl_* ---
add_executable(test-tpb-run-pli EXCLUDE_FROM_ALL
    tpb_run_pli.c
    mock_kernel.c
    mock_timer.c
    mock_dynloader.c
)
target_include_directories(test-tpb-run-pli PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test-tpb-run-pli PRIVATE tpbench m)
set_target_properties(test-tpb-run-pli PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${TPB_TEST_RUNTIME_OUTPUT_DIRECTORY}"
    LIBRARY_OUTPUT_DIRECTORY "${TPB_TEST_LIBRARY_OUTPUT_DIRECTORY}"
    ARCHIVE_OUTPUT_DIRECTORY "${TPB_TEST_ARCHIVE_OUTPUT_DIRECTORY}"
)

# Pack A2 sub-cases
tpb_add_subcase(A2 A2.1 null_handle        test-tpb-run-pli corelib)
tpb_add_subcase(A2 A2.2 missing_exec       test-tpb-run-pli corelib)
tpb_add_subcase(A2 A2.3 basic_success      test-tpb-run-pli corelib)
tpb_add_subcase(A2 A2.4 child_nonzero_exit test-tpb-run-pli corelib)
tpb_add_subcase(A2 A2.5 child_signaled     test-tpb-run-pli corelib)

# --- Strftime test executable (Pack A3) ---
add_executable(test-strftime EXCLUDE_FROM_ALL
    test_strftime.c
    mock_kernel.c
    mock_timer.c
)
target_include_directories(test-strftime PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test-strftime PRIVATE tpbench m)
set_target_properties(test-strftime PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${TPB_TEST_RUNTIME_OUTPUT_DIRECTORY}"
    LIBRARY_OUTPUT_DIRECTORY "${TPB_TEST_LIBRARY_OUTPUT_DIRECTORY}"
    ARCHIVE_OUTPUT_DIRECTORY "${TPB_TEST_ARCHIVE_OUTPUT_DIRECTORY}"
)

# Pack A3 sub-cases
tpb_add_subcase(A3 A3.1  get_datetime       test-strftime corelib)
tpb_add_subcase(A3 A3.2  get_btime          test-strftime corelib)
tpb_add_subcase(A3 A3.3  btime_to_datetime  test-strftime corelib)
tpb_add_subcase(A3 A3.4  datetime_to_bits   test-strftime corelib)
tpb_add_subcase(A3 A3.5  bits_to_datetime   test-strftime corelib)
tpb_add_subcase(A3 A3.6  roundtrip_bits     test-strftime corelib)
tpb_add_subcase(A3 A3.7  bits_to_isoutc     test-strftime corelib)
tpb_add_subcase(A3 A3.8  bits_to_isotz_utc  test-strftime corelib)
tpb_add_subcase(A3 A3.9  bits_to_isotz_offset test-strftime corelib)
tpb_add_subcase(A3 A3.10 isoutc_to_bits     test-strftime corelib)
tpb_add_subcase(A3 A3.11 isotz_to_bits      test-strftime corelib)
tpb_add_subcase(A3 A3.12 roundtrip_isoutc   test-strftime corelib)
tpb_add_subcase(A3 A3.13 roundtrip_isotz    test-strftime corelib)
tpb_add_subcase(A3 A3.14 invalid_mode       test-strftime corelib)
tpb_add_subcase(A3 A3.15 null_pointer       test-strftime corelib)
tpb_add_subcase(A3 A3.16 year_range         test-strftime corelib)
tpb_add_subcase(A3 A3.17 timezone_bias      test-strftime corelib)

# Aggregate targets
add_custom_target(test_corelib
    DEPENDS test-tpb-run-fli test-tpb-run-pli test-strftime
)

add_custom_target(check_corelib
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -L "^corelib$"
    DEPENDS test_corelib
    USES_TERMINAL
)
