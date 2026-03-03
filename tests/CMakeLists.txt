set(TPB_TEST_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/bin")
set(TPB_TEST_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/lib")
set(TPB_TEST_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/lib")

add_subdirectory(corelib)
add_subdirectory(tpbcli)

add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_tpbcli test_corelib
    USES_TERMINAL
)
