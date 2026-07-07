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

# Depends on tpbcli and stream kernel; omit when TPB_KERNELS excludes stream
if(TARGET tpbk_stream)
    add_dependencies(test-tri-record tpbcli tpbcli-pli-launcher tpbk_stream)
    tpb_add_subcase(C1 C1.1 tri_tests_record test-tri-record integration)
    tpb_add_subcase(C1 C1.2 benchmark_tbatch_record test-tri-record integration)
    tpb_add_subcase(C1 C1.3 benchmark_begin_batch_fail test-tri-record integration)
    tpb_add_subcase(C1 C1.4 benchmark_metric_missing_soft_fail test-tri-record integration)
    tpb_add_subcase(C1 C1.5 rtenv_fk_and_counter test-tri-record integration)
    tpb_add_subcase(C1 C1.6 rtenv_fallback_warn test-tri-record integration)
    tpb_add_subcase(C1 C1.7 rtenv_kenvs_input_param test-tri-record integration)
endif()

# --- Integration: O2/O3 kernel compile history (Pack C3) ---
if(TARGET tpbk_stream)
    add_test(
        NAME integration.C3.1_kernel_o2_o3
        COMMAND "${CMAKE_SOURCE_DIR}/tests/integration/run_kernel_o2_o3.sh"
    )
    set_tests_properties(integration.C3.1_kernel_o2_o3 PROPERTIES
        LABELS "integration")
endif()

# --- Integration: CMake package + kernel template staging (Pack C4) ---
add_test(
    NAME integration.C4.1_cmake_template_staging
    COMMAND "${CMAKE_SOURCE_DIR}/tests/integration/run_cmake_template_install.sh"
)
set_tests_properties(integration.C4.1_cmake_template_staging PROPERTIES
    LABELS "integration")

# --- Integration: template kernel init/build active switching (Pack C4) ---
add_test(
    NAME integration.C4.2_kernel_template_switch
    COMMAND "${CMAKE_SOURCE_DIR}/tests/integration/run_kernel_template_switch.sh"
)
set_tests_properties(integration.C4.2_kernel_template_switch PROPERTIES
    LABELS "integration")

# --- Integration: kernel registry list/build (Pack C5) ---
add_test(
    NAME integration.C5.1_kernel_list_registry
    COMMAND "${CMAKE_SOURCE_DIR}/tests/integration/run_kernel_list_registry.sh"
)
set_tests_properties(integration.C5.1_kernel_list_registry PROPERTIES
    LABELS "integration")

# Aggregate targets (tri-record only when stream kernel is in the build)
set(_tpb_integration_depends "")
if(TARGET tpbk_stream)
    list(APPEND _tpb_integration_depends test-tri-record)
endif()
add_custom_target(test_integration
    DEPENDS ${_tpb_integration_depends}
)

set(_tpb_check_integration_kernels "")
if(TARGET tpbk_stream)
    list(APPEND _tpb_check_integration_kernels tpbk_stream)
endif()
add_custom_target(check_integration
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -L "^integration$"
    DEPENDS test_integration tpbcli tpbcli-pli-launcher tpbench copy_cmake_package copy_yaml_files ${_tpb_check_integration_kernels}
    USES_TERMINAL
)
