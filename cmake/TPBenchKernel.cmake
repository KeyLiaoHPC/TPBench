# TPBenchKernel.cmake
#
# Helper function for building TPBench kernels.
# Used both in-tree (from src/kernels/) and out-of-tree (via find_package(TPBench)).
#
# Usage:
#   tpbench_add_kernel(
#       NAME        mykern
#       SOURCES     tpbk_mykern.c
#       MAIN_SOURCE tpbk_mykern_main.c
#       [LINK_LIBS  extra_lib ...]
#   )
#
# Creates two targets:
#   tpbk_<name>      - shared library (.so)
#   tpbk_<name>.tpbx - PLI executable

if(NOT DEFINED CMAKE_INSTALL_LIBDIR)
    set(CMAKE_INSTALL_LIBDIR lib)
endif()
if(NOT DEFINED CMAKE_INSTALL_BINDIR)
    set(CMAKE_INSTALL_BINDIR bin)
endif()

function(tpbench_add_kernel)
    cmake_parse_arguments(_K "" "NAME;MAIN_SOURCE" "SOURCES;LINK_LIBS" ${ARGN})

    if(NOT _K_NAME)
        message(FATAL_ERROR "tpbench_add_kernel: NAME is required")
    endif()
    if(NOT _K_SOURCES)
        message(FATAL_ERROR "tpbench_add_kernel: SOURCES is required")
    endif()
    if(NOT _K_MAIN_SOURCE)
        message(FATAL_ERROR "tpbench_add_kernel: MAIN_SOURCE is required")
    endif()

    set(_lib_target  "tpbk_${_K_NAME}")
    set(_exec_target "tpbk_${_K_NAME}.tpbx")

    # Shared library
    add_library(${_lib_target} SHARED ${_K_SOURCES})
    target_link_libraries(${_lib_target} PRIVATE tpbench m)
    if(_K_LINK_LIBS)
        target_link_libraries(${_lib_target} PRIVATE ${_K_LINK_LIBS})
    endif()

    # PLI executable
    add_executable(${_exec_target} ${_K_MAIN_SOURCE})
    target_link_libraries(${_exec_target} PRIVATE tpbench ${_lib_target} m)
    if(_K_LINK_LIBS)
        target_link_libraries(${_exec_target} PRIVATE ${_K_LINK_LIBS})
    endif()

    # Install rules
    install(TARGETS ${_lib_target}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(TARGETS ${_exec_target}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endfunction()
