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

# Helper: register one sub-case for an already-defined test executable.
#   _prefix  : CTest name prefix (e.g. "A1")
#   _case_id : Case id passed as argv[1] (e.g. "A1.1")
#   _case_name: Human label appended to CTest name (e.g. "basic_success")
#   _target  : CMake build target name
#   _label   : CTest label
macro(tpb_add_subcase _prefix _case_id _case_name _target _label)
    add_test(
        NAME "${_case_id}-${_case_name}"
        COMMAND ${CMAKE_COMMAND}
            -DBUILD_DIR=${CMAKE_BINARY_DIR}
            -DBUILD_TARGET=${_target}
            -DTEST_EXECUTABLE=$<TARGET_FILE:${_target}>
            -DTEST_ARGS=${_case_id}
            -P ${CMAKE_SOURCE_DIR}/tests/RunBuiltTest.cmake
    )
    set_tests_properties("${_case_id}-${_case_name}" PROPERTIES LABELS "${_label}")
endmacro()

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

# Aggregate targets
add_custom_target(test_corelib
    DEPENDS test-tpb-run-fli test-tpb-run-pli
)

add_custom_target(check_corelib
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -L "^corelib$"
    DEPENDS test_corelib
    USES_TERMINAL
)
