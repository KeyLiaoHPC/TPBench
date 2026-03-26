set(TPB_TEST_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/bin")
set(TPB_TEST_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/lib")
set(TPB_TEST_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/lib")

add_compile_options(-O2)

# Helper: register one sub-case for an already-defined test executable.
#   _prefix   : CTest name prefix (e.g. "A1")
#   _case_id  : Case id passed as argv[1] (e.g. "A1.1")
#   _case_name: Human label appended to CTest name (e.g. "basic_success")
#   _target   : CMake build target name
#   _label    : CTest label
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

add_subdirectory(corelib)
add_subdirectory(tpbcli)
add_subdirectory(integration)

add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_tpbcli test_corelib
    USES_TERMINAL
)
