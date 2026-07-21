# Post-build script: register kernel and write compilation metadata.
cmake_minimum_required(VERSION 3.16)

if(NOT TGT_FILE OR NOT KERNEL_NAME OR NOT TPBCLI)
    message(FATAL_ERROR "TPBenchKernelPostBuildRun.cmake: TGT_FILE, KERNEL_NAME, TPBCLI required")
endif()

if(APPLE)
    set(_tpb_shlib_ext ".dylib")
else()
    set(_tpb_shlib_ext ".so")
endif()

set(_tpb_env_cmd ${CMAKE_COMMAND} -E env)
if(DEFINED TPB_HOME AND NOT TPB_HOME STREQUAL "")
    list(APPEND _tpb_env_cmd TPB_HOME=${TPB_HOME})
endif()
if(DEFINED ENV{TPB_WORKSPACE} AND NOT "$ENV{TPB_WORKSPACE}" STREQUAL "")
    list(APPEND _tpb_env_cmd TPB_WORKSPACE=$ENV{TPB_WORKSPACE})
elseif(DEFINED TPB_HOME AND NOT TPB_HOME STREQUAL "")
    list(APPEND _tpb_env_cmd TPB_WORKSPACE=${TPB_HOME})
endif()

execute_process(
    COMMAND ${_tpb_env_cmd}
        ${TPBCLI} kernel set --kernel ${KERNEL_NAME}
        --key variation.target "${TGT_FILE}"
        --key variation.build_type "${BUILD_TYPE}"
        --key compilation.compiler.id "${C_COMPILER_ID}"
        --key compilation.compiler.version "${C_COMPILER_VERSION}"
        --key compilation.compiler.path "${C_COMPILER}"
        --key compilation.c_flags "${C_FLAGS}"
        --key compilation.kernel_cflags "${KERNEL_CFLAGS}"
        --key dependency.tpbench "libtpbench${_tpb_shlib_ext}"
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _out
    ERROR_VARIABLE _err)
if(NOT _rc EQUAL 0)
    message(WARNING "kernel set compile metadata failed (${_rc}): ${_err}${_out}")
endif()
