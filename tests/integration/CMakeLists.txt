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

# --- Integration test: merge_pthread (Pack C2) ---
add_executable(test-merge-pthread EXCLUDE_FROM_ALL
    test_merge_pthread.c
)
target_include_directories(test-merge-pthread PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test-merge-pthread PRIVATE tpbench m pthread)
set_target_properties(test-merge-pthread PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${TPB_TEST_RUNTIME_OUTPUT_DIRECTORY}"
    LIBRARY_OUTPUT_DIRECTORY "${TPB_TEST_LIBRARY_OUTPUT_DIRECTORY}"
    ARCHIVE_OUTPUT_DIRECTORY "${TPB_TEST_ARCHIVE_OUTPUT_DIRECTORY}"
)
tpb_add_subcase(C2 C2.1 merge_pthread test-merge-pthread integration)

# --- Integration test: merge_fork (Pack C2) ---
add_executable(test-merge-fork EXCLUDE_FROM_ALL
    test_merge_fork.c
)
target_include_directories(test-merge-fork PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test-merge-fork PRIVATE tpbench m)
set_target_properties(test-merge-fork PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${TPB_TEST_RUNTIME_OUTPUT_DIRECTORY}"
    LIBRARY_OUTPUT_DIRECTORY "${TPB_TEST_LIBRARY_OUTPUT_DIRECTORY}"
    ARCHIVE_OUTPUT_DIRECTORY "${TPB_TEST_ARCHIVE_OUTPUT_DIRECTORY}"
)
tpb_add_subcase(C2 C2.2 merge_fork test-merge-fork integration)

# --- Integration test: merge_hybrid (Pack C2) ---
add_executable(test-merge-hybrid EXCLUDE_FROM_ALL
    test_merge_hybrid.c
)
target_include_directories(test-merge-hybrid PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test-merge-hybrid PRIVATE tpbench m pthread)
set_target_properties(test-merge-hybrid PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${TPB_TEST_RUNTIME_OUTPUT_DIRECTORY}"
    LIBRARY_OUTPUT_DIRECTORY "${TPB_TEST_LIBRARY_OUTPUT_DIRECTORY}"
    ARCHIVE_OUTPUT_DIRECTORY "${TPB_TEST_ARCHIVE_OUTPUT_DIRECTORY}"
)
tpb_add_subcase(C2 C2.3 merge_hybrid test-merge-hybrid integration)

# Aggregate targets
add_custom_target(test_integration
    DEPENDS test-tri-record test-merge-pthread test-merge-fork test-merge-hybrid
)

add_custom_target(check_integration
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -L "^integration$"
    DEPENDS test_integration tpbcli tpbk_stream.tpbx
    USES_TERMINAL
)
