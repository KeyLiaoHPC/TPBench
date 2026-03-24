# TPBenchGpuKernelsRocm.cmake
#
# Include only inside root CMakeLists.txt when TPB_USE_ROCM is ON,
# after enable_language(HIP) and find_package(hip REQUIRED).
# Requires TPB_ROCM_KERNEL_DEFS from TPBenchKernelRegistry.cmake.

foreach(_def IN LISTS TPB_ROCM_KERNEL_DEFS)
    string(REPLACE "|" ";" _parts "${_def}")
    list(LENGTH _parts _n)
    if(NOT _n EQUAL 6)
        message(FATAL_ERROR "TPB_ROCM_KERNEL_DEFS entry must have 6 '|'-separated fields: ${_def}")
    endif()
    list(GET _parts 0 _rk_name)
    list(GET _parts 3 _rk_backend)
    list(GET _parts 4 _rk_hip_src)
    list(GET _parts 5 _rk_pli_src)

    if(NOT _rk_backend STREQUAL "rocm")
        message(FATAL_ERROR "Unknown ROCm BACKEND '${_rk_backend}' for kernel '${_rk_name}' (expected rocm)")
    endif()

    set(_lib_target "tpbk_${_rk_name}")
    set(_exec_target "tpbk_${_rk_name}.tpbx")
    set(_hip_path "${CMAKE_SOURCE_DIR}/${_rk_hip_src}")
    set(_pli_path "${CMAKE_SOURCE_DIR}/${_rk_pli_src}")

    add_library(${_lib_target} SHARED "${_hip_path}")
    target_include_directories(${_lib_target} PRIVATE "${CMAKE_SOURCE_DIR}/src")
    target_link_libraries(${_lib_target} PRIVATE tpbench hip::device m)
    set_target_properties(${_lib_target} PROPERTIES
        HIP_SEPARABLE_COMPILATION ON
        POSITION_INDEPENDENT_CODE ON)
    if(TPB_SHOW_DEBUG)
        target_compile_definitions(${_lib_target} PRIVATE TPB_K_DEBUG=1)
        message(STATUS "TPB_K_DEBUG enabled for ${_lib_target}")
    endif()

    add_executable(${_exec_target} "${_pli_path}")
    target_include_directories(${_exec_target} PRIVATE "${CMAKE_SOURCE_DIR}/src")
    target_link_libraries(${_exec_target} PRIVATE tpbench ${_lib_target} hip::host m)
    set_target_properties(${_exec_target} PROPERTIES
        LINKER_LANGUAGE CXX
        CXX_STANDARD 11)
    if(TPB_SHOW_DEBUG)
        target_compile_definitions(${_exec_target} PRIVATE TPB_K_DEBUG=1)
        message(STATUS "TPB_K_DEBUG enabled for ${_exec_target}")
    endif()

    tpb_set_install_rpath_tpbench_shlib(${_lib_target})
    tpb_set_install_rpath_tpbench_exe(${_exec_target})

    install(TARGETS ${_lib_target}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(TARGETS ${_exec_target}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endforeach()
