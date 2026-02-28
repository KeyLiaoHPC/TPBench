add_executable(test-cli-run-dimargs EXCLUDE_FROM_ALL
    test-cli-run-dimargs.c
    ${CMAKE_SOURCE_DIR}/src/tpbcli-run-dim.c
)

target_link_libraries(test-cli-run-dimargs PRIVATE tpbench m)

set_target_properties(test-cli-run-dimargs PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${TPB_TEST_RUNTIME_OUTPUT_DIRECTORY}"
    LIBRARY_OUTPUT_DIRECTORY "${TPB_TEST_LIBRARY_OUTPUT_DIRECTORY}"
    ARCHIVE_OUTPUT_DIRECTORY "${TPB_TEST_ARCHIVE_OUTPUT_DIRECTORY}"
)

add_test(
    NAME cli-run-dimargs
    COMMAND ${CMAKE_COMMAND}
        -DBUILD_DIR=${CMAKE_BINARY_DIR}
        -DBUILD_TARGET=test-cli-run-dimargs
        -DTEST_EXECUTABLE=$<TARGET_FILE:test-cli-run-dimargs>
        -P ${CMAKE_SOURCE_DIR}/tests/RunBuiltTest.cmake
)
set_tests_properties(cli-run-dimargs PROPERTIES LABELS tpbcli)

add_custom_target(test_tpbcli
    DEPENDS test-cli-run-dimargs
)

add_custom_target(check_tpbcli
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -L "^tpbcli$"
    DEPENDS test_tpbcli
    USES_TERMINAL
)
