# TPBenchBaseRtenvRun.cmake
# Idempotently create base runtime environment (id=0) in the build workspace.

if(NOT TPB_CREATE_BASE_RTENV)
    return()
endif()

if(NOT TPBCLI)
    message(FATAL_ERROR "TPBenchBaseRtenvRun.cmake requires TPBCLI")
endif()

if(NOT TPB_HOME)
    set(TPB_HOME "${CMAKE_BINARY_DIR}")
endif()

execute_process(
    COMMAND ${CMAKE_COMMAND} -E env TPB_WORKSPACE=${TPB_HOME}
            ${TPBCLI} rtenv init-base
    RESULT_VARIABLE _tpb_rtenv_init_rc
    OUTPUT_QUIET
    ERROR_QUIET
)

if(NOT _tpb_rtenv_init_rc EQUAL 0)
    message(WARNING "tpbcli rtenv init-base failed (rc=${_tpb_rtenv_init_rc})")
endif()
