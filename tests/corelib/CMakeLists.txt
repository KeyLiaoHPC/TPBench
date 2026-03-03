# FLI test (Pack A1)
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

add_test(
    NAME corelib-run-fli
    COMMAND ${CMAKE_COMMAND}
        -DBUILD_DIR=${CMAKE_BINARY_DIR}
        -DBUILD_TARGET=test-tpb-run-fli
        -DTEST_EXECUTABLE=$<TARGET_FILE:test-tpb-run-fli>
        -P ${CMAKE_SOURCE_DIR}/tests/RunBuiltTest.cmake
)
set_tests_properties(corelib-run-fli PROPERTIES LABELS corelib)

# PLI test (Pack A2) -- mock_dynloader.c interposes tpb_dl_* symbols
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

add_test(
    NAME corelib-run-pli
    COMMAND ${CMAKE_COMMAND}
        -DBUILD_DIR=${CMAKE_BINARY_DIR}
        -DBUILD_TARGET=test-tpb-run-pli
        -DTEST_EXECUTABLE=$<TARGET_FILE:test-tpb-run-pli>
        -P ${CMAKE_SOURCE_DIR}/tests/RunBuiltTest.cmake
)
set_tests_properties(corelib-run-pli PROPERTIES LABELS corelib)

# Aggregate targets
add_custom_target(test_corelib
    DEPENDS test-tpb-run-fli test-tpb-run-pli
)

add_custom_target(check_corelib
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -L "^corelib$"
    DEPENDS test_corelib
    USES_TERMINAL
)
