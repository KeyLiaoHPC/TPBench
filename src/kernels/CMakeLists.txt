# ============================================================================
# TPBench Kernel Build System
#
# Standalone kernel definitions with name/tag-based selection.
#
# Selection variables (set from parent or command line):
#   TPB_KERNELS      Comma-separated kernel names, "all", or "default" (default: "default")
#   TPB_KERNEL_TAGS  Comma-separated tags; kernels matching ANY listed tag are included
#
# Per-kernel tag override:
#   TPB_KERNEL_<NAME>_TAGS  Override default tags for a specific kernel
#
# Examples:
#   cmake -B build                                       # only stream
#   cmake -B build -DTPB_KERNELS=all                     # all CPU kernels
#   cmake -B build -DTPB_KERNELS=stream,triad            # specific kernels
#   cmake -B build -DTPB_KERNEL_TAGS=bandwidth           # all bandwidth kernels
#   cmake -B build -DTPB_KERNEL_TAGS=stanza              # staxpy + striad
#   cmake -B build -DTPB_KERNELS=default -DTPB_KERNEL_TAGS=stanza  # stream + staxpy + striad
# ============================================================================

include(${CMAKE_SOURCE_DIR}/cmake/TPBenchKernelRegistry.cmake)

set(TPB_KERNELS "default" CACHE STRING "Kernel selection: comma-separated names, 'all', or 'default'")
set(TPB_KERNEL_TAGS "" CACHE STRING "Comma-separated tags to select kernels (union with TPB_KERNELS)")

# ---------------------------------------------------------------------------
# CPU kernel registry: TPB_CPU_KERNEL_DEFS in TPBenchKernelRegistry.cmake
#
# Each entry: NAME|DEFAULT_TAGS|EXTRA_LINK_LIBS|CONDITION_VAR
# Source convention (single-file kernel):
#   Both .so and .tpbx are built from simple/tpbk_<name>.c
#   The .tpbx target defines TPB_K_BUILD_MAIN to enable main()
# ---------------------------------------------------------------------------
# Parse selection inputs into CMake lists
# ---------------------------------------------------------------------------
string(REPLACE "," ";" _tpb_sel_names "${TPB_KERNELS}")
string(REPLACE "," ";" _tpb_sel_tags  "${TPB_KERNEL_TAGS}")

# Accumulate built kernel targets for the parent scope
set(TPB_BUILT_KERNEL_LIBS "")
set(TPB_BUILT_KERNEL_EXES "")
set(TPB_BUILT_KERNEL_NAMES "")
set(TPB_SKIPPED_KERNEL_NAMES "")

foreach(_def IN LISTS TPB_CPU_KERNEL_DEFS)
    # Split on '|'
    string(REPLACE "|" ";" _parts "${_def}")
    list(GET _parts 0 _kname)
    list(GET _parts 1 _ktags_str)
    list(GET _parts 2 _kextra)
    list(GET _parts 3 _kcond)

    # --- Check platform condition ---
    if(NOT "${_kcond}" STREQUAL "")
        if(NOT ${_kcond})
            continue()
        endif()
    endif()

    # --- Resolve effective tags (allow per-kernel override) ---
    string(TOUPPER "${_kname}" _kname_upper)
    if(DEFINED TPB_KERNEL_${_kname_upper}_TAGS)
        string(REPLACE "," ";" _ktags "${TPB_KERNEL_${_kname_upper}_TAGS}")
    else()
        string(REPLACE "," ";" _ktags "${_ktags_str}")
    endif()

    # --- Decide whether to build this kernel ---
    set(_build OFF)

    # Match by explicit name
    if("all" IN_LIST _tpb_sel_names)
        set(_build ON)
    elseif("${_kname}" IN_LIST _tpb_sel_names)
        set(_build ON)
    elseif("default" IN_LIST _tpb_sel_names AND "default" IN_LIST _ktags)
        set(_build ON)
    endif()

    # Match by tag
    if(NOT _build AND NOT "${_tpb_sel_tags}" STREQUAL "")
        foreach(_tag IN LISTS _tpb_sel_tags)
            if("${_tag}" IN_LIST _ktags)
                set(_build ON)
                break()
            endif()
        endforeach()
    endif()

    if(NOT _build)
        list(APPEND TPB_SKIPPED_KERNEL_NAMES "${_kname}")
        continue()
    endif()

    # --- Create targets ---
    set(_src_dir "${CMAKE_CURRENT_SOURCE_DIR}/simple")
    set(_lib_target  "tpbk_${_kname}")
    set(_exec_target "tpbk_${_kname}.tpbx")

    # Shared library (.so)
    add_library(${_lib_target} SHARED "${_src_dir}/tpbk_${_kname}.c")
    target_include_directories(${_lib_target} PRIVATE "${CMAKE_SOURCE_DIR}/src")
    target_link_libraries(${_lib_target} PRIVATE tpbench m)
    if(OpenMP_C_FOUND)
        target_link_libraries(${_lib_target} PRIVATE OpenMP::OpenMP_C)
    endif()
    if(NOT "${_kextra}" STREQUAL "")
        target_link_libraries(${_lib_target} PRIVATE ${_kextra})
    endif()
    if(TPB_SHOW_DEBUG)
        target_compile_definitions(${_lib_target} PRIVATE TPB_K_DEBUG=1)
    endif()

    # PLI executable (.tpbx) — same source, with TPB_K_BUILD_MAIN to enable main()
    add_executable(${_exec_target} "${_src_dir}/tpbk_${_kname}.c")
    target_compile_definitions(${_exec_target} PRIVATE TPB_K_BUILD_MAIN)
    target_include_directories(${_exec_target} PRIVATE "${CMAKE_SOURCE_DIR}/src")
    target_link_libraries(${_exec_target} PRIVATE tpbench m)
    if(OpenMP_C_FOUND)
        target_link_libraries(${_exec_target} PRIVATE OpenMP::OpenMP_C)
    endif()
    if(NOT "${_kextra}" STREQUAL "")
        target_link_libraries(${_exec_target} PRIVATE ${_kextra})
    endif()
    if(TPB_SHOW_DEBUG)
        target_compile_definitions(${_exec_target} PRIVATE TPB_K_DEBUG=1)
    endif()

    # Install rules
    install(TARGETS ${_lib_target}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(TARGETS ${_exec_target}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

    list(APPEND TPB_BUILT_KERNEL_LIBS  "${_lib_target}")
    list(APPEND TPB_BUILT_KERNEL_EXES  "${_exec_target}")
    list(APPEND TPB_BUILT_KERNEL_NAMES "${_kname}")
endforeach()

# Export to parent scope
set(TPB_BUILT_KERNEL_LIBS  "${TPB_BUILT_KERNEL_LIBS}"  PARENT_SCOPE)
set(TPB_BUILT_KERNEL_EXES  "${TPB_BUILT_KERNEL_EXES}"  PARENT_SCOPE)
set(TPB_BUILT_KERNEL_NAMES "${TPB_BUILT_KERNEL_NAMES}" PARENT_SCOPE)

# --- Summary ---
string(REPLACE ";" ", " _built_str   "${TPB_BUILT_KERNEL_NAMES}")
string(REPLACE ";" ", " _skipped_str "${TPB_SKIPPED_KERNEL_NAMES}")
message(STATUS "TPBench kernel selection (TPB_KERNELS=${TPB_KERNELS}):")
message(STATUS "  Built:   ${_built_str}")
if(NOT "${_skipped_str}" STREQUAL "")
    message(STATUS "  Skipped: ${_skipped_str}")
endif()
message(STATUS "  (Use -DTPB_KERNELS=all or -DTPB_KERNEL_TAGS=... to enable more)")
