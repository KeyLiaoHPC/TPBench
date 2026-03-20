# TPBenchCmakeHelp.cmake
# Generate ${CMAKE_BINARY_DIR}/tpb_cmake_help.txt for target tpb_cmake_help.
# Call from root CMakeLists.txt after cache options and add_subdirectory(src/kernels) exist.

include(${CMAKE_SOURCE_DIR}/cmake/TPBenchKernelRegistry.cmake)

# When a cache entry is set from the command line (-DVAR=...), CMake replaces HELPSTRING
# with "No help, variable specified on the command line." — use these as stable fallbacks.
set(_tpb_cmake_help_doc_lines
    "TPB_DIR|TPBench installation directory for kernel discovery"
    "TPB_KERNELS|Kernel selection: comma-separated names, 'all', or 'default'"
    "TPB_KERNEL_TAGS|Comma-separated tags to select kernels (union with TPB_KERNELS)"
    "TPB_SHOW_DEBUG|Enable kernel debug logging (TPB_K_DEBUG)"
    "TPB_USE_AVX512|Enable AVX-512 SIMD instructions"
    "TPB_USE_AVX2|Enable AVX2 SIMD instructions"
    "TPB_USE_KP_SVE|Enable ARM SVE SIMD instructions"
    "TPB_USE_OPENMP|Enable OpenMP parallelization"
    "TPB_USE_MPI|MPI support: OFF to disable, ON for system default, or specify MPI installation path"
    "TPB_USE_ROCM|Enable ROCm/HIP GPU support"
)

set(_out "${CMAKE_BINARY_DIR}/tpb_cmake_help.txt")
set(_buf "")

string(APPEND _buf "Available build options:\n")

get_cmake_property(_cache_vars CACHE_VARIABLES)
set(_tpb_opts "")
foreach(_v ${_cache_vars})
    if(_v MATCHES "^TPB_")
        list(APPEND _tpb_opts "${_v}")
    endif()
endforeach()
list(SORT _tpb_opts)

set(_maxlen 0)
foreach(_v ${_tpb_opts})
    string(LENGTH "${_v}" _len)
    if(_len GREATER _maxlen)
        set(_maxlen ${_len})
    endif()
endforeach()
math(EXPR _col "${_maxlen} + 4")

foreach(_v ${_tpb_opts})
    get_property(_help CACHE ${_v} PROPERTY HELPSTRING)
    if("${_help}" STREQUAL "" OR _help MATCHES "No help, variable specified on the command line")
        set(_fb "")
        foreach(_line ${_tpb_cmake_help_doc_lines})
            string(REPLACE "|" ";" _pair "${_line}")
            list(GET _pair 0 _key)
            if(_key STREQUAL _v)
                list(LENGTH _pair _pair_len)
                if(_pair_len GREATER 1)
                    list(GET _pair 1 _fb)
                endif()
                break()
            endif()
        endforeach()
        if(NOT "${_fb}" STREQUAL "")
            set(_help "${_fb}")
        elseif(_v MATCHES "^TPB_KERNEL_.*_TAGS$")
            set(_help "Override default tags for that kernel (see src/kernels/CMakeLists.txt)")
        else()
            set(_help "(no description; search CMakeLists.txt for ${_v})")
        endif()
    endif()
    string(LENGTH "${_v}" _vlen)
    math(EXPR _need "${_col} - ${_vlen}")
    if(_need LESS 1)
        set(_need 1)
    endif()
    string(REPEAT " " ${_need} _sp)
    string(APPEND _buf "  ${_v}${_sp}${_help}\n")
endforeach()

string(APPEND _buf "\nAvailable kernels:\n")
string(APPEND _buf "  name, tags, compile prerequisites\n")

foreach(_def IN LISTS TPB_CPU_KERNEL_DEFS)
    string(REPLACE "|" ";" _parts "${_def}")
    list(GET _parts 0 _kname)
    list(GET _parts 1 _ktags)
    list(GET _parts 3 _kcond)
    if("${_kcond}" STREQUAL "")
        set(_pre "Always")
    elseif(_kcond STREQUAL "MPI_C_FOUND")
        set(_pre "TPB_USE_MPI and MPI found")
    else()
        set(_pre "${_kcond}")
    endif()
    string(APPEND _buf "  ${_kname}, ${_ktags}, ${_pre}\n")
endforeach()

foreach(_def IN LISTS TPB_ROCM_KERNEL_DEFS)
    string(REPLACE "|" ";" _parts "${_def}")
    list(GET _parts 0 _kname)
    list(GET _parts 1 _ktags)
    list(GET _parts 2 _pre)
    string(APPEND _buf "  ${_kname}, ${_ktags}, ${_pre}\n")
endforeach()

file(WRITE "${_out}" "${_buf}")
