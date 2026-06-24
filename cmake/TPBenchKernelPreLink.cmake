# Backup existing kernel .so before link overwrites it.
cmake_minimum_required(VERSION 3.16)

if(NOT TGT_FILE OR NOT KERNEL_NAME)
    message(FATAL_ERROR "TPBenchKernelPreLink.cmake: TGT_FILE and KERNEL_NAME required")
endif()

get_filename_component(_lib_dir "${TGT_FILE}" DIRECTORY)
set(_inactive_dir "${_lib_dir}/inactive")
set(_so_name "libtpbk_${KERNEL_NAME}.so")

if(NOT EXISTS "${_lib_dir}/${_so_name}")
    return()
endif()

file(MAKE_DIRECTORY "${_inactive_dir}")

if(TPBCLI AND EXISTS "${TPBCLI}")
    set(_tpb_env_cmd ${CMAKE_COMMAND} -E env)
    if(DEFINED ENV{TPB_WORKSPACE})
        list(APPEND _tpb_env_cmd TPB_WORKSPACE=$ENV{TPB_WORKSPACE})
    endif()
    execute_process(
        COMMAND ${_tpb_env_cmd}
            ${TPBCLI} kernel backup-inactive --kernel ${KERNEL_NAME}
        RESULT_VARIABLE _bak_rc
        OUTPUT_VARIABLE _bak_out
        ERROR_VARIABLE _bak_err)
    if(NOT _bak_rc EQUAL 0)
        message(WARNING "kernel backup-inactive failed: ${_bak_err}")
    endif()
else()
    # Fallback: move with unknown KernelID placeholder
    file(RENAME "${_lib_dir}/${_so_name}"
         "${_inactive_dir}/libkernel_${KERNEL_NAME}_unknown.so_bak")
endif()
