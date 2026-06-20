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

    install(TARGETS ${_lib_target}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
endfunction()
