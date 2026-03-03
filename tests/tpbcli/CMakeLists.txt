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
tpb_add_subcase(B1 B1.4 nested_expansion test-cli-run-dimargs tpbcli)
tpb_add_subcase(B1 B1.5 total_count      test-cli-run-dimargs tpbcli)

# Aggregate targets
add_custom_target(test_tpbcli
    DEPENDS test-cli-run-dimargs
)

add_custom_target(check_tpbcli
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -L "^tpbcli$"
    DEPENDS test_tpbcli
    USES_TERMINAL
)
