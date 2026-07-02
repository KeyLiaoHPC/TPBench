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

# --- rafdb test executable (Pack A4) ---
add_executable(test-raf EXCLUDE_FROM_ALL
    test_raf.c
)
target_include_directories(test-raf PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test-raf PRIVATE tpbench m)
set_target_properties(test-raf PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${TPB_TEST_RUNTIME_OUTPUT_DIRECTORY}"
    LIBRARY_OUTPUT_DIRECTORY "${TPB_TEST_LIBRARY_OUTPUT_DIRECTORY}"
    ARCHIVE_OUTPUT_DIRECTORY "${TPB_TEST_ARCHIVE_OUTPUT_DIRECTORY}"
)

# Pack A4 sub-cases
tpb_add_subcase(A4 A4.1  magic_construct    test-raf corelib)
tpb_add_subcase(A4 A4.2  magic_validate_ok  test-raf corelib)
tpb_add_subcase(A4 A4.3  magic_validate_bad test-raf corelib)
tpb_add_subcase(A4 A4.4  magic_scan_tpbe    test-raf corelib)
tpb_add_subcase(A4 A4.5  magic_scan_tpbr    test-raf corelib)
tpb_add_subcase(A4 A4.6  id_tbatch          test-raf corelib)
tpb_add_subcase(A4 A4.7  id_kernel          test-raf corelib)
tpb_add_subcase(A4 A4.8  id_task            test-raf corelib)
tpb_add_subcase(A4 A4.9  id_uniqueness      test-raf corelib)
tpb_add_subcase(A4 A4.10 entry_tbatch       test-raf corelib)
tpb_add_subcase(A4 A4.11 entry_kernel       test-raf corelib)
tpb_add_subcase(A4 A4.12 entry_task         test-raf corelib)
tpb_add_subcase(A4 A4.13 entry_multi        test-raf corelib)
tpb_add_subcase(A4 A4.14 record_tbatch      test-raf corelib)
tpb_add_subcase(A4 A4.15 record_kernel      test-raf corelib)
tpb_add_subcase(A4 A4.16 record_task        test-raf corelib)
tpb_add_subcase(A4 A4.17 header_1d          test-raf corelib)
tpb_add_subcase(A4 A4.18 header_multidim    test-raf corelib)
tpb_add_subcase(A4 A4.19 header_mixed       test-raf corelib)

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
tpb_add_subcase(A5 A5.7  skip_unalloc_output   test-1d-array corelib)

# --- Task capsule tests (Pack A6) ---
add_executable(test-capsule EXCLUDE_FROM_ALL
    test_capsule.c
)
target_include_directories(test-capsule PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test-capsule PRIVATE tpbench m rt)
set_target_properties(test-capsule PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${TPB_TEST_RUNTIME_OUTPUT_DIRECTORY}"
    LIBRARY_OUTPUT_DIRECTORY "${TPB_TEST_LIBRARY_OUTPUT_DIRECTORY}"
    ARCHIVE_OUTPUT_DIRECTORY "${TPB_TEST_ARCHIVE_OUTPUT_DIRECTORY}"
)

tpb_add_subcase(A6 A6.1  id_taskcapsule_differs test-capsule corelib)
tpb_add_subcase(A6 A6.2  write_task_id_out      test-capsule corelib)
tpb_add_subcase(A6 A6.3  write_task_null_out    test-capsule corelib)
tpb_add_subcase(A6 A6.4  create_capsule_header  test-capsule corelib)
tpb_add_subcase(A6 A6.5  tpbe_lists_capsule     test-capsule corelib)
tpb_add_subcase(A6 A6.6  append_once            test-capsule corelib)
tpb_add_subcase(A6 A6.7  append_multi           test-capsule corelib)
tpb_add_subcase(A6 A6.8  subprocess_append      test-capsule corelib)
tpb_add_subcase(A6 A6.9  sync_capsule_task        test-capsule corelib)

# --- kernel record tests (Pack A7) ---
add_executable(test-kernel-record EXCLUDE_FROM_ALL
    test-kernel-record.c
)
target_include_directories(test-kernel-record PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test-kernel-record PRIVATE tpbench m)
set_target_properties(test-kernel-record PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${TPB_TEST_RUNTIME_OUTPUT_DIRECTORY}"
    LIBRARY_OUTPUT_DIRECTORY "${TPB_TEST_LIBRARY_OUTPUT_DIRECTORY}"
    ARCHIVE_OUTPUT_DIRECTORY "${TPB_TEST_ARCHIVE_OUTPUT_DIRECTORY}"
)

tpb_add_subcase(A7 A7.1 build_attr_counts test-kernel-record corelib)
if(TARGET tpbk_stream)
    target_compile_definitions(test-kernel-record PRIVATE
        TPB_TEST_STREAM_SO=1
        "TPB_TEST_TPB_HOME=\"${CMAKE_BINARY_DIR}\"")
    add_dependencies(test-kernel-record tpbk_stream)
    tpb_add_subcase(A7 A7.2 stream_register_record test-kernel-record corelib)
endif()

# --- tpblog test executable (Pack A8) ---
add_executable(test-tpblog EXCLUDE_FROM_ALL
    test_tpblog.c
)
target_include_directories(test-tpblog PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test-tpblog PRIVATE tpbench m)
set_target_properties(test-tpblog PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${TPB_TEST_RUNTIME_OUTPUT_DIRECTORY}"
    LIBRARY_OUTPUT_DIRECTORY "${TPB_TEST_LIBRARY_OUTPUT_DIRECTORY}"
    ARCHIVE_OUTPUT_DIRECTORY "${TPB_TEST_ARCHIVE_OUTPUT_DIRECTORY}"
)

tpb_add_subcase(A8 A8.1 init_session_header test-tpblog corelib)
tpb_add_subcase(A8 A8.2 log_file_append      test-tpblog corelib)
tpb_add_subcase(A8 A8.3 dual_write           test-tpblog corelib)
tpb_add_subcase(A8 A8.4 tag_labels           test-tpblog corelib)
tpb_add_subcase(A8 A8.5 column_widths        test-tpblog corelib)
tpb_add_subcase(A8 A8.6 column_output        test-tpblog corelib)
tpb_add_subcase(A8 A8.7 degenerate_output    test-tpblog corelib)
tpb_add_subcase(A8 A8.8 snprintf_macro       test-tpblog corelib)

# Aggregate targets
add_custom_target(test_corelib
    DEPENDS test-tpb-run-pli test-strftime test-raf test-1d-array test-capsule
            test-kernel-record test-tpblog
)

add_custom_target(check_corelib
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -L "^corelib$"
    DEPENDS test_corelib
    USES_TERMINAL
)
