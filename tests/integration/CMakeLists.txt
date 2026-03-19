# --- Integration test: tri_tests_record (Pack C1) ---
add_executable(test-tri-record EXCLUDE_FROM_ALL
    test_tri_tests_record.c
)
target_include_directories(test-tri-record PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test-tri-record PRIVATE tpbench m)
set_target_properties(test-tri-record PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${TPB_TEST_RUNTIME_OUTPUT_DIRECTORY}"
    LIBRARY_OUTPUT_DIRECTORY "${TPB_TEST_LIBRARY_OUTPUT_DIRECTORY}"
    ARCHIVE_OUTPUT_DIRECTORY "${TPB_TEST_ARCHIVE_OUTPUT_DIRECTORY}"
)

# The integration test depends on tpbcli and stream kernel being built
add_dependencies(test-tri-record tpbcli tpbk_stream.tpbx)

# Pack C1 sub-cases
tpb_add_subcase(C1 C1.1 tri_tests_record test-tri-record integration)

# Aggregate targets
add_custom_target(test_integration
    DEPENDS test-tri-record
)

add_custom_target(check_integration
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -L "^integration$"
    DEPENDS test_integration tpbcli tpbk_stream.tpbx
    USES_TERMINAL
)
