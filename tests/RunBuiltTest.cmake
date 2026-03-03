if(NOT DEFINED BUILD_DIR)
    message(FATAL_ERROR "BUILD_DIR is required")
endif()

if(NOT DEFINED BUILD_TARGET)
    message(FATAL_ERROR "BUILD_TARGET is required")
endif()

if(NOT DEFINED TEST_EXECUTABLE)
    message(FATAL_ERROR "TEST_EXECUTABLE is required")
endif()

set(build_command
    "${CMAKE_COMMAND}"
    --build "${BUILD_DIR}"
    --target "${BUILD_TARGET}"
)

if(DEFINED CTEST_CONFIGURATION_TYPE AND NOT CTEST_CONFIGURATION_TYPE STREQUAL "")
    list(APPEND build_command --config "${CTEST_CONFIGURATION_TYPE}")
endif()

execute_process(
    COMMAND ${build_command}
    RESULT_VARIABLE build_result
)

if(NOT build_result EQUAL 0)
    message(FATAL_ERROR "Failed to build target ${BUILD_TARGET}")
endif()

set(run_command "${TEST_EXECUTABLE}")
if(DEFINED TEST_ARGS)
    list(APPEND run_command ${TEST_ARGS})
endif()

execute_process(
    COMMAND ${run_command}
    RESULT_VARIABLE test_result
)

if(NOT test_result EQUAL 0)
    message(FATAL_ERROR "Test executable failed with exit code ${test_result}")
endif()
