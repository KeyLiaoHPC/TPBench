# NPB-MPI extra sources for in-tree and out-of-tree kernel builds.
# Directory of this file (not the caller): src/kernels/npb_mpi or the install copy.
set(TPB_NPB_MPI_DIR "${CMAKE_CURRENT_LIST_DIR}")

function(tpb_npb_mpi_sources _kname _outvar)
    set(_here "${TPB_NPB_MPI_DIR}")
    set(_common "${_here}/common")
    set(_vendor "${_here}/vendor")
    set(_srcs "")

    set(_tpb_f90 "")
    if(EXISTS "${_here}/../../include/tpb.F90")
        set(_tpb_f90 "${_here}/../../include/tpb.F90")
    elseif(EXISTS "${_here}/../../../include/tpb.F90")
        set(_tpb_f90 "${_here}/../../../include/tpb.F90")
    endif()

    list(APPEND _srcs
        "${_common}/tpb_npb_timers.c"
        "${_common}/tpb_npb_class.c"
        "${_common}/tpb_npb_result.c"
        "${_common}/tpb_npb_pli.c"
    )

    if("${_kname}" STREQUAL "is_mpi")
        list(APPEND _srcs
            "${_vendor}/IS/is.c"
            "${_vendor}/common/c_print_results.c")
        set(${_outvar} "${_srcs}" PARENT_SCOPE)
        return()
    endif()

    if("${_kname}" STREQUAL "dt_mpi")
        list(APPEND _srcs
            "${_vendor}/DT/dt.c"
            "${_vendor}/DT/DGraph.c"
            "${_vendor}/common/c_print_results.c"
            "${_vendor}/common/randdp.c")
        set(${_outvar} "${_srcs}" PARENT_SCOPE)
        return()
    endif()

    if(NOT _tpb_f90 STREQUAL "")
        list(APPEND _srcs "${_tpb_f90}")
    endif()
    list(APPEND _srcs
        "${_common}/tpb_npb_timers.f90"
        "${_common}/tpb_npb_result_f.f90"
        "${_vendor}/common/print_results.f90"
        "${_vendor}/common/randi8.f90"
    )

    if(NOT "${_kname}" STREQUAL "ep_mpi")
        list(APPEND _srcs "${_vendor}/common/get_active_nprocs.f90")
    endif()

    set(_bench "")
    if("${_kname}" STREQUAL "bt_mpi")
        set(_bench "BT")
    elseif("${_kname}" STREQUAL "sp_mpi")
        set(_bench "SP")
    elseif("${_kname}" STREQUAL "lu_mpi")
        set(_bench "LU")
    elseif("${_kname}" STREQUAL "cg_mpi")
        set(_bench "CG")
    elseif("${_kname}" STREQUAL "mg_mpi")
        set(_bench "MG")
    elseif("${_kname}" STREQUAL "ft_mpi")
        set(_bench "FT")
    elseif("${_kname}" STREQUAL "ep_mpi")
        set(_bench "EP")
    endif()

    if(_bench STREQUAL "")
        message(FATAL_ERROR "tpb_npb_mpi_sources: unknown kernel ${_kname}")
    endif()

    file(GLOB _kf90 "${_vendor}/${_bench}/*.f90")
    foreach(_f IN LISTS _kf90)
        get_filename_component(_fn "${_f}" NAME)
        if(_fn MATCHES "_vec\\.f90$")
            continue()
        endif()
        if(_fn STREQUAL "mpinpb_f.f90" OR _fn STREQUAL "mpinpb_f08.f90")
            continue()
        endif()
        if(_bench STREQUAL "BT")
            if(_fn STREQUAL "full_mpiio.f90" OR _fn STREQUAL "simple_mpiio.f90"
               OR _fn STREQUAL "epio.f90" OR _fn STREQUAL "fortran_io.f90"
               OR _fn STREQUAL "btio_common.f90" OR _fn STREQUAL "bt_data_vec.f90")
                continue()
            endif()
        endif()
        list(APPEND _srcs "${_f}")
    endforeach()

    set(${_outvar} "${_srcs}" PARENT_SCOPE)
endfunction()

function(tpb_npb_mpi_bench_dir _kname _outvar)
    if("${_kname}" STREQUAL "bt_mpi")
        set(${_outvar} "BT" PARENT_SCOPE)
    elseif("${_kname}" STREQUAL "sp_mpi")
        set(${_outvar} "SP" PARENT_SCOPE)
    elseif("${_kname}" STREQUAL "lu_mpi")
        set(${_outvar} "LU" PARENT_SCOPE)
    elseif("${_kname}" STREQUAL "cg_mpi")
        set(${_outvar} "CG" PARENT_SCOPE)
    elseif("${_kname}" STREQUAL "mg_mpi")
        set(${_outvar} "MG" PARENT_SCOPE)
    elseif("${_kname}" STREQUAL "ft_mpi")
        set(${_outvar} "FT" PARENT_SCOPE)
    elseif("${_kname}" STREQUAL "ep_mpi")
        set(${_outvar} "EP" PARENT_SCOPE)
    elseif("${_kname}" STREQUAL "is_mpi")
        set(${_outvar} "IS" PARENT_SCOPE)
    elseif("${_kname}" STREQUAL "dt_mpi")
        set(${_outvar} "DT" PARENT_SCOPE)
    else()
        set(${_outvar} "" PARENT_SCOPE)
    endif()
endfunction()
