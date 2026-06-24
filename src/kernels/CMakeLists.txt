# ============================================================================
# TPBench Kernel Build System
#
# TPB_KERNELS / TPB_KERNEL_TAGS are defined in the root CMakeLists.txt.
# Per-kernel tag override: TPB_KERNEL_<NAME>_TAGS
# ============================================================================

include(${CMAKE_SOURCE_DIR}/cmake/TPBenchKernelSelect.cmake)

if(TPB_RECORD_KERNEL_COMPILE_HISTORY)
    include(${CMAKE_SOURCE_DIR}/cmake/TPBenchKernelPostBuild.cmake)
endif()

set(TPB_KERNEL_CFLAGS "" CACHE STRING "C compile options for CPU kernels (empty = -O2 only)")
set(TPB_KERNEL_CXXFLAGS "" CACHE STRING "C++/HIP compile options for ROCm kernels (empty = -O2 only)")
set(TPB_KERNEL_FFLAGS "" CACHE STRING "Fortran compile options for Fortran kernels (reserved; empty = -O2 only)")

function(tpb_kernel_apply_c_options _tgt)
    if("${TPB_KERNEL_CFLAGS}" STREQUAL "")
        target_compile_options(${_tgt} PRIVATE $<$<COMPILE_LANGUAGE:C>:-O2>)
    else()
        separate_arguments(_tpb_kcf UNIX_COMMAND "${TPB_KERNEL_CFLAGS}")
        target_compile_options(${_tgt} PRIVATE $<$<COMPILE_LANGUAGE:C>:${_tpb_kcf}>)
    endif()
endfunction()

# ---------------------------------------------------------------------------
# CPU kernel registry: TPB_CPU_KERNEL_DEFS in TPBenchKernelRegistry.cmake
# Each entry: NAME|DEFAULT_TAGS|EXTRA_LINK_LIBS|CONDITION_VAR
# ---------------------------------------------------------------------------
set(TPB_BUILT_KERNEL_LIBS "")
set(TPB_BUILT_KERNEL_EXES "")
set(TPB_BUILT_KERNEL_NAMES "")
set(TPB_SKIPPED_KERNEL_NAMES "")

foreach(_def IN LISTS TPB_CPU_KERNEL_DEFS)
    string(REPLACE "|" ";" _parts "${_def}")
    list(GET _parts 0 _kname)
    list(GET _parts 1 _ktags_str)
    list(GET _parts 2 _kextra)
    list(GET _parts 3 _kcond)

    if(NOT "${_kcond}" STREQUAL "")
        if(NOT ${_kcond})
            continue()
        endif()
    endif()

    tpb_kernel_should_build("${_kname}" "${_ktags_str}" "${_tpb_sel_names}" "${_tpb_sel_tags}" _build)
    if(NOT _build)
        list(APPEND TPB_SKIPPED_KERNEL_NAMES "${_kname}")
        continue()
    endif()

    file(GLOB _possible_sources "${CMAKE_CURRENT_SOURCE_DIR}/*/tpbk_${_kname}.c")
    list(LENGTH _possible_sources _src_count)
    if(_src_count EQUAL 0)
        message(FATAL_ERROR "Kernel source tpbk_${_kname}.c not found in any subdirectory")
    elseif(_src_count GREATER 1)
        message(WARNING "Multiple sources found for kernel ${_kname}; using first: ${_possible_sources}")
    endif()
    list(GET _possible_sources 0 _kernel_source)

    set(_so_target "tpbk_${_kname}")

    add_library(${_so_target} SHARED "${_kernel_source}")
    target_include_directories(${_so_target} PRIVATE "${CMAKE_SOURCE_DIR}/src")
    target_link_libraries(${_so_target} PRIVATE tpbench m)
    set_target_properties(${_so_target} PROPERTIES
        OUTPUT_NAME "tpbk_${_kname}"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    )
    if(UNIX)
        target_link_options(${_so_target} PRIVATE "-Wl,--export-dynamic")
    endif()
    tpb_kernel_apply_c_options(${_so_target})
    if(TPB_ENABLE_OPENMP AND OpenMP_C_FOUND)
        target_link_libraries(${_so_target} PRIVATE OpenMP::OpenMP_C)
    endif()
    if(NOT "${_kextra}" STREQUAL "")
        target_link_libraries(${_so_target} PRIVATE ${_kextra})
    endif()
    if(TPB_SHOW_DEBUG)
        target_compile_definitions(${_so_target} PRIVATE TPB_K_DEBUG=1)
    endif()

    tpb_set_install_rpath_tpbench_shlib(${_so_target})

    install(TARGETS ${_so_target}
            COMPONENT tpbench_kernels
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

    list(APPEND TPB_BUILT_KERNEL_LIBS  "${_so_target}")
    list(APPEND TPB_BUILT_KERNEL_NAMES "${_kname}")

    if(TPB_RECORD_KERNEL_COMPILE_HISTORY)
        tpb_kernel_add_compile_history(${_so_target} ${_kname})
    endif()
endforeach()

set(TPB_BUILT_KERNEL_LIBS  "${TPB_BUILT_KERNEL_LIBS}"  PARENT_SCOPE)
set(TPB_BUILT_KERNEL_EXES  "${TPB_BUILT_KERNEL_EXES}"  PARENT_SCOPE)
set(TPB_BUILT_KERNEL_NAMES "${TPB_BUILT_KERNEL_NAMES}" PARENT_SCOPE)

string(REPLACE ";" ", " _built_str   "${TPB_BUILT_KERNEL_NAMES}")
string(REPLACE ";" ", " _skipped_str "${TPB_SKIPPED_KERNEL_NAMES}")
message(STATUS "TPBench kernel selection (TPB_KERNELS=${TPB_KERNELS}):")
message(STATUS "  Built:   ${_built_str}")
if(NOT "${_skipped_str}" STREQUAL "")
    message(STATUS "  Skipped: ${_skipped_str}")
endif()
message(STATUS "  (Use -DTPB_KERNELS=all or -DTPB_KERNEL_TAGS=... to enable more)")
