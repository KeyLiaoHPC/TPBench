# TPBenchKernelPostBuild.cmake
#
# Include after tpbcli target exists. Adds PRE_LINK inactive backup and
# POST_BUILD compile-history metadata registration for kernel targets.

function(tpb_kernel_add_compile_history _tgt _kname)
    if(NOT TARGET tpbcli)
        message(FATAL_ERROR "tpb_kernel_add_compile_history requires tpbcli target")
    endif()

    add_dependencies(${_tgt} tpbcli)

    add_custom_command(TARGET ${_tgt} PRE_LINK
        COMMAND ${CMAKE_COMMAND}
            -DTGT_FILE=$<TARGET_FILE:${_tgt}>
            -DKERNEL_NAME=${_kname}
            -DTPBCLI=$<TARGET_FILE:tpbcli>
            -P ${CMAKE_SOURCE_DIR}/cmake/TPBenchKernelPreLink.cmake
        COMMENT "Backup prior libtpbk_${_kname}.so to lib/inactive if present"
        VERBATIM)

    add_custom_command(TARGET ${_tgt} POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DTGT_FILE=$<TARGET_FILE:${_tgt}>
            -DKERNEL_NAME=${_kname}
            -DTPBCLI=$<TARGET_FILE:tpbcli>
            -DC_COMPILER=${CMAKE_C_COMPILER}
            -DC_COMPILER_ID=${CMAKE_C_COMPILER_ID}
            -DC_COMPILER_VERSION=${CMAKE_C_COMPILER_VERSION}
            -DC_FLAGS=${CMAKE_C_FLAGS}
            -DKERNEL_CFLAGS=${TPB_KERNEL_CFLAGS}
            -DBUILD_TYPE=${CMAKE_BUILD_TYPE}
            -P ${CMAKE_SOURCE_DIR}/cmake/TPBenchKernelPostBuildRun.cmake
        COMMENT "Register compile metadata for libtpbk_${_kname}.so"
        VERBATIM)
endfunction()
