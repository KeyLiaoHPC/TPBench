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
tpb_add_subcase(A2 A2.6 kernel_id_env      test-tpb-run-pli corelib)

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

# --- rawdb test executable (Pack A4) ---
add_executable(test-rawdb EXCLUDE_FROM_ALL
    test_rawdb.c
)
target_include_directories(test-rawdb PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test-rawdb PRIVATE tpbench m)
set_target_properties(test-rawdb PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${TPB_TEST_RUNTIME_OUTPUT_DIRECTORY}"
    LIBRARY_OUTPUT_DIRECTORY "${TPB_TEST_LIBRARY_OUTPUT_DIRECTORY}"
    ARCHIVE_OUTPUT_DIRECTORY "${TPB_TEST_ARCHIVE_OUTPUT_DIRECTORY}"
)

# Pack A4 sub-cases
tpb_add_subcase(A4 A4.1  magic_construct    test-rawdb corelib)
tpb_add_subcase(A4 A4.2  magic_validate_ok  test-rawdb corelib)
tpb_add_subcase(A4 A4.3  magic_validate_bad test-rawdb corelib)
tpb_add_subcase(A4 A4.4  magic_scan_tpbe    test-rawdb corelib)
tpb_add_subcase(A4 A4.5  magic_scan_tpbr    test-rawdb corelib)
tpb_add_subcase(A4 A4.6  id_tbatch          test-rawdb corelib)
tpb_add_subcase(A4 A4.7  id_kernel          test-rawdb corelib)
tpb_add_subcase(A4 A4.8  id_task            test-rawdb corelib)
tpb_add_subcase(A4 A4.9  id_uniqueness      test-rawdb corelib)
tpb_add_subcase(A4 A4.10 entry_tbatch       test-rawdb corelib)
tpb_add_subcase(A4 A4.11 entry_kernel       test-rawdb corelib)
tpb_add_subcase(A4 A4.12 entry_task         test-rawdb corelib)
tpb_add_subcase(A4 A4.13 entry_multi        test-rawdb corelib)
tpb_add_subcase(A4 A4.14 record_tbatch      test-rawdb corelib)
tpb_add_subcase(A4 A4.15 record_kernel      test-rawdb corelib)
tpb_add_subcase(A4 A4.16 record_task        test-rawdb corelib)
tpb_add_subcase(A4 A4.17 header_1d          test-rawdb corelib)
tpb_add_subcase(A4 A4.18 header_multidim    test-rawdb corelib)
tpb_add_subcase(A4 A4.19 header_mixed       test-rawdb corelib)

# --- 1D array write/read test (Pack A5) ---
add_executable(test-1d-array EXCLUDE_FROM_ALL
    test_1d_array_write.c
    mock_kernel.c
    mock_timer.c
)
target_include_directories(test-1d-array PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test-1d-array PRIVATE tpbench m)
set_target_properties(test-1d-array PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${TPB_TEST_RUNTIME_OUTPUT_DIRECTORY}"
    LIBRARY_OUTPUT_DIRECTORY "${TPB_TEST_LIBRARY_OUTPUT_DIRECTORY}"
    ARCHIVE_OUTPUT_DIRECTORY "${TPB_TEST_ARCHIVE_OUTPUT_DIRECTORY}"
)

# Pack A5 sub-cases
tpb_add_subcase(A5 A5.1  write_read_doubles    test-1d-array corelib)
tpb_add_subcase(A5 A5.2  write_read_floats     test-1d-array corelib)
tpb_add_subcase(A5 A5.3  write_read_int64      test-1d-array corelib)
tpb_add_subcase(A5 A5.4  write_read_int32      test-1d-array corelib)
tpb_add_subcase(A5 A5.5  write_read_random_len test-1d-array corelib)
tpb_add_subcase(A5 A5.6  write_read_multi      test-1d-array corelib)

# Aggregate targets
add_custom_target(test_corelib
    DEPENDS test-tpb-run-pli test-strftime test-rawdb test-1d-array
)

add_custom_target(check_corelib
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -L "^corelib$"
    DEPENDS test_corelib
    USES_TERMINAL
)
