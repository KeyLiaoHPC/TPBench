# TPBenchKernel.cmake
#
# Helper function for building TPBench kernels.
# Used both in-tree (from src/kernels/) and out-of-tree (via find_package(TPBench)).
#
# Usage:
#   tpbench_add_kernel(
#       NAME        mykern
#       SOURCES     tpbk_mykern.c
#       [LINK_LIBS  extra_lib ...]
#   )
#
# Creates one shared library target:
#   tpbk_<name> - libtpbk_<name>.so with tpbk_<name>_entry exported

if(NOT DEFINED CMAKE_INSTALL_LIBDIR)
    set(CMAKE_INSTALL_LIBDIR lib)
endif()
if(NOT DEFINED CMAKE_INSTALL_BINDIR)
    set(CMAKE_INSTALL_BINDIR bin)
endif()

set(TPB_KERNEL_CFLAGS "" CACHE STRING "C compile options for out-of-tree kernels")
set(TPB_KERNEL_CXXFLAGS "" CACHE STRING "C++/HIP compile options for out-of-tree kernels")
set(TPB_KERNEL_FFLAGS "" CACHE STRING "Fortran compile options for out-of-tree kernels")

function(_tpbench_kernel_apply_c_options _tgt)
    if("${TPB_KERNEL_CFLAGS}" STREQUAL "")
        target_compile_options(${_tgt} PRIVATE $<$<COMPILE_LANGUAGE:C>:-O2>)
    else()
        separate_arguments(_tpb_kcf UNIX_COMMAND "${TPB_KERNEL_CFLAGS}")
        target_compile_options(${_tgt} PRIVATE $<$<COMPILE_LANGUAGE:C>:${_tpb_kcf}>)
    endif()
endfunction()

function(_tpbench_kernel_apply_cxx_options _tgt)
    if("${TPB_KERNEL_CXXFLAGS}" STREQUAL "")
        target_compile_options(${_tgt} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-O2>)
    else()
        separate_arguments(_tpb_kcxf UNIX_COMMAND "${TPB_KERNEL_CXXFLAGS}")
        target_compile_options(${_tgt} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${_tpb_kcxf}>)
    endif()
endfunction()

function(_tpbench_kernel_apply_fortran_options _tgt)
    if("${TPB_KERNEL_FFLAGS}" STREQUAL "")
        target_compile_options(${_tgt} PRIVATE $<$<COMPILE_LANGUAGE:Fortran>:-O2>)
    else()
        separate_arguments(_tpb_kff UNIX_COMMAND "${TPB_KERNEL_FFLAGS}")
        target_compile_options(${_tgt} PRIVATE $<$<COMPILE_LANGUAGE:Fortran>:${_tpb_kff}>)
    endif()
endfunction()

function(tpbench_add_kernel)
    cmake_parse_arguments(_K "" "NAME" "SOURCES;LINK_LIBS" ${ARGN})

    if(NOT _K_NAME)
        message(FATAL_ERROR "tpbench_add_kernel: NAME is required")
    endif()
    if(NOT _K_SOURCES)
        message(FATAL_ERROR "tpbench_add_kernel: SOURCES is required")
    endif()

    set(_lib_target "tpbk_${_K_NAME}")

    add_library(${_lib_target} SHARED ${_K_SOURCES})
    target_link_libraries(${_lib_target} PRIVATE tpbench m)
    if(_K_LINK_LIBS)
        target_link_libraries(${_lib_target} PRIVATE ${_K_LINK_LIBS})
    endif()

    set_target_properties(${_lib_target} PROPERTIES
        OUTPUT_NAME "tpbk_${_K_NAME}"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    )
    if(UNIX)
        target_link_options(${_lib_target} PRIVATE "-Wl,--export-dynamic")
    endif()

    _tpbench_kernel_apply_c_options(${_lib_target})
    _tpbench_kernel_apply_cxx_options(${_lib_target})
    _tpbench_kernel_apply_fortran_options(${_lib_target})

    install(TARGETS ${_lib_target}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
endfunction()
