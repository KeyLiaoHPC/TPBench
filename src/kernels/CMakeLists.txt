# ============================================================================
# TPBench Kernel Build System
#
# TPB_KERNELS / TPB_KERNEL_TAGS are defined in the root CMakeLists.txt.
# Per-kernel tag override: TPB_KERNEL_<NAME>_TAGS
# CPU kernel catalog: kernel_list.cmake.in (NAME|TAGS|PATH)
# ============================================================================

include(${CMAKE_SOURCE_DIR}/cmake/TPBenchKernelSelect.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/TPBenchKernelRegistry.cmake)

if(TPB_RECORD_KERNEL_COMPILE_HISTORY)
    include(${CMAKE_SOURCE_DIR}/cmake/TPBenchKernelPostBuild.cmake)
endif()

set(TPB_KERNEL_CFLAGS "" CACHE STRING "C compile options for CPU kernels (empty = -O2 only)")
set(TPB_KERNEL_CXXFLAGS "" CACHE STRING "C++/HIP compile options for ROCm kernels (empty = -O2 only)")
set(TPB_KERNEL_FFLAGS "" CACHE STRING "Fortran compile options for Fortran kernels (reserved; empty = -O2 only)")
set(TPB_KERNEL_LDFLAGS "" CACHE STRING "Link options for CPU kernels (empty = none)")

function(tpb_kernel_apply_c_options _tgt)
    if("${TPB_KERNEL_CFLAGS}" STREQUAL "")
        target_compile_options(${_tgt} PRIVATE $<$<COMPILE_LANGUAGE:C>:-O2>)
    else()
        separate_arguments(_tpb_kcf UNIX_COMMAND "${TPB_KERNEL_CFLAGS}")
        target_compile_options(${_tgt} PRIVATE $<$<COMPILE_LANGUAGE:C>:${_tpb_kcf}>)
    endif()
endfunction()

function(tpb_kernel_apply_ld_options _tgt)
    if(NOT "${TPB_KERNEL_LDFLAGS}" STREQUAL "")
        separate_arguments(_tpb_klf UNIX_COMMAND "${TPB_KERNEL_LDFLAGS}")
        target_link_options(${_tgt} PRIVATE ${_tpb_klf})
    endif()
endfunction()

function(tpb_kernel_lookup_link_def _kname _extra_out _cond_out)
    set(_extra "")
    set(_cond "")
    foreach(_def IN LISTS TPB_CPU_KERNEL_LINK_DEFS)
        string(REPLACE "|" ";" _parts "${_def}")
        list(GET _parts 0 _ln)
        if("${_ln}" STREQUAL "${_kname}")
            list(LENGTH _parts _n)
            if(_n GREATER 1)
                list(GET _parts 1 _extra)
            endif()
            if(_n GREATER 2)
                list(GET _parts 2 _cond)
            endif()
            break()
        endif()
    endforeach()
    set(${_extra_out} "${_extra}" PARENT_SCOPE)
    set(${_cond_out} "${_cond}" PARENT_SCOPE)
endfunction()

# ---------------------------------------------------------------------------
# Parse kernel_list.cmake.in: NAME|TAGS|PATH
# ---------------------------------------------------------------------------
set(TPB_BUILT_KERNEL_LIBS "")
set(TPB_BUILT_KERNEL_EXES "")
set(TPB_BUILT_KERNEL_NAMES "")
set(TPB_SKIPPED_KERNEL_NAMES "")
set(_tpb_all_registry_paths "")

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/kernel_list.cmake.in"
     _tpb_kernel_list_lines)

# Collect every unique registry PATH (including kernels not built in-tree)
foreach(_line IN LISTS _tpb_kernel_list_lines)
    string(STRIP "${_line}" _line)
    if("${_line}" STREQUAL "" OR _line MATCHES "^#")
        continue()
    endif()
    string(REPLACE "|" ";" _parts "${_line}")
    list(LENGTH _parts _nparts)
    if(_nparts LESS 3)
        message(FATAL_ERROR "Invalid kernel_list entry (need NAME|TAGS|PATH): ${_line}")
    endif()
    list(GET _parts 2 _kpath)
    if(NOT "${_kpath}" IN_LIST _tpb_all_registry_paths)
        list(APPEND _tpb_all_registry_paths "${_kpath}")
    endif()
endforeach()

# Out-of-tree kernel build: stage registry CMakeLists.txt for every PATH
foreach(_kpath IN LISTS _tpb_all_registry_paths)
    install(FILES
        ${CMAKE_SOURCE_DIR}/cmake/templates/kernel/CMakeLists.registry.txt
        DESTINATION src/kernels/${_kpath}
        RENAME CMakeLists.txt
        COMPONENT tpbench_kernels)
endforeach()

# kernel_link_defs.txt: NAME|EXTRA_LINK_LIBS for tpbcli kernel build
set(_tpb_kernel_link_defs_path
    "${CMAKE_BINARY_DIR}/src/kernels/kernel_link_defs.txt")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/src/kernels")
file(WRITE "${_tpb_kernel_link_defs_path}" "# NAME|EXTRA_LINK_LIBS\n")
foreach(_def IN LISTS TPB_CPU_KERNEL_LINK_DEFS)
    string(REPLACE "|" ";" _lparts "${_def}")
    list(GET _lparts 0 _ln)
    list(GET _lparts 1 _lex)
    file(APPEND "${_tpb_kernel_link_defs_path}" "${_ln}|${_lex}\n")
endforeach()
install(FILES "${_tpb_kernel_link_defs_path}"
        DESTINATION src/kernels
        COMPONENT tpbench_kernels)

foreach(_line IN LISTS _tpb_kernel_list_lines)
    string(STRIP "${_line}" _line)
    if("${_line}" STREQUAL "" OR _line MATCHES "^#")
        continue()
    endif()
    string(REPLACE "|" ";" _parts "${_line}")
    list(LENGTH _parts _nparts)
    if(_nparts LESS 3)
        message(FATAL_ERROR "Invalid kernel_list entry (need NAME|TAGS|PATH): ${_line}")
    endif()
    list(GET _parts 0 _kname)
    list(GET _parts 1 _ktags_str)
    list(GET _parts 2 _kpath)

    tpb_kernel_lookup_link_def("${_kname}" _kextra _kcond)

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

    set(_kernel_source "${CMAKE_CURRENT_SOURCE_DIR}/${_kpath}/tpbk_${_kname}.c")
    if(NOT EXISTS "${_kernel_source}")
        message(FATAL_ERROR "Kernel source not found: ${_kernel_source}")
    endif()

    set(_so_target "tpbk_${_kname}")

    add_library(${_so_target} SHARED "${_kernel_source}")
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
    tpb_kernel_apply_ld_options(${_so_target})
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

# Stage kernel tree under ${CMAKE_BINARY_DIR}/src/kernels for build-tree tpbcli
file(GLOB _tpb_kern_subdirs RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
     "${CMAKE_CURRENT_SOURCE_DIR}/*")
foreach(_sd IN LISTS _tpb_kern_subdirs)
    if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${_sd}"
       AND NOT _sd STREQUAL "build")
        file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/${_sd}
             DESTINATION ${CMAKE_BINARY_DIR}/src/kernels)
    endif()
endforeach()
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/kernel_list.cmake.in
     DESTINATION ${CMAKE_BINARY_DIR}/src/kernels)
file(COPY "${_tpb_kernel_link_defs_path}"
     DESTINATION ${CMAKE_BINARY_DIR}/src/kernels)
# Build-tree mirror of registry CMakeLists.txt (all PATHs, not only built)
foreach(_kpath IN LISTS _tpb_all_registry_paths)
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/src/kernels/${_kpath}")
    configure_file(
        ${CMAKE_SOURCE_DIR}/cmake/templates/kernel/CMakeLists.registry.txt
        ${CMAKE_BINARY_DIR}/src/kernels/${_kpath}/CMakeLists.txt
        COPYONLY)
endforeach()

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
        DESTINATION src/kernels)
