# TPBenchInstallRafdb.cmake
# Sync build-tree rafdb metadata (kernel/, runtime_environment/) into TPB_HOME on
# full cmake --install. Does not read or write TPB_WORKSPACE.

cmake_minimum_required(VERSION 3.16)

if(CMAKE_INSTALL_COMPONENT AND NOT CMAKE_INSTALL_COMPONENT STREQUAL "")
    return()
endif()

if(DEFINED ENV{TPB_RAFDB_INSTALL_SYNC_DONE} AND NOT "$ENV{TPB_RAFDB_INSTALL_SYNC_DONE}" STREQUAL "")
    return()
endif()
set(ENV{TPB_RAFDB_INSTALL_SYNC_DONE} "1")

if(NOT DEFINED TBP_BUILD_DIR OR TBP_BUILD_DIR STREQUAL "")
    message(FATAL_ERROR "TPBenchInstallRafdb.cmake: TBP_BUILD_DIR is not set")
endif()

# --- helpers ----------------------------------------------------------------

function(tpb_rafdb_resolve_tpb_home out_var)
    if(DEFINED ENV{TPB_HOME} AND NOT "$ENV{TPB_HOME}" STREQUAL "")
        set(_home "$ENV{TPB_HOME}")
    elseif(DEFINED ENV{HOME} AND NOT "$ENV{HOME}" STREQUAL "")
        set(_home "$ENV{HOME}/.tpbench")
        message(STATUS "TPB_HOME not set; using ${_home} for rafdb install sync")
    else()
        message(FATAL_ERROR
            "TPB_HOME is not set and HOME is unavailable; cannot sync rafdb")
    endif()
    set(${out_var} "${_home}" PARENT_SCOPE)
endfunction()

function(tpb_rafdb_resolve_install_root out_var)
    if(DEFINED ENV{DESTDIR} AND NOT "$ENV{DESTDIR}" STREQUAL "")
        set(_root "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}")
    else()
        set(_root "${CMAKE_INSTALL_PREFIX}")
    endif()
    get_filename_component(_root "${_root}" ABSOLUTE)
    set(${out_var} "${_root}" PARENT_SCOPE)
endfunction()

function(tpb_rafdb_has_data dir out_var)
    if(NOT EXISTS "${dir}")
        set(${out_var} FALSE PARENT_SCOPE)
        return()
    endif()
    file(GLOB_RECURSE _tpb_data_files
        "${dir}/*.tpbe"
        "${dir}/*.tpbr")
    if(_tpb_data_files)
        set(${out_var} TRUE PARENT_SCOPE)
    else()
        set(${out_var} FALSE PARENT_SCOPE)
    endif()
endfunction()

function(tpb_rafdb_read_base_id config_path out_var)
    set(_val 1)
    if(EXISTS "${config_path}")
        file(READ "${config_path}" _content)
        if(_content MATCHES "\"base_id\"[ \t]*:[ \t]*([0-9]+)")
            set(_val "${CMAKE_MATCH_1}")
        endif()
    endif()
    set(${out_var} ${_val} PARENT_SCOPE)
endfunction()

function(tpb_rafdb_read_config_name config_path out_var)
    set(_name "default")
    if(EXISTS "${config_path}")
        file(READ "${config_path}" _content)
        if(_content MATCHES "\"name\"[ \t]*:[ \t]*\"([^\"]+)\"")
            set(_name "${CMAKE_MATCH_1}")
        endif()
    endif()
    set(${out_var} "${_name}" PARENT_SCOPE)
endfunction()

function(tpb_rafdb_merge_config src_ws dst_ws)
    set(_src_cfg "${src_ws}/etc/config.json")
    set(_dst_cfg "${dst_ws}/etc/config.json")

    if(NOT EXISTS "${_src_cfg}")
        return()
    endif()

    tpb_rafdb_read_base_id("${_src_cfg}" _base_id)

    if(NOT EXISTS "${_dst_cfg}")
        file(MAKE_DIRECTORY "${dst_ws}/etc")
        file(COPY "${_src_cfg}" DESTINATION "${dst_ws}/etc")
        message(STATUS "TPBench rafdb sync: copied etc/config.json to ${dst_ws}")
        return()
    endif()

    tpb_rafdb_read_config_name("${_dst_cfg}" _name)
    file(WRITE "${_dst_cfg}"
"{
    \"name\": \"${_name}\",
    \"runtime_environment\": {
        \"base_id\": ${_base_id}
    }
}
")
    message(STATUS "TPBench rafdb sync: merged runtime_environment.base_id=${_base_id} into ${_dst_cfg}")
endfunction()

function(tpb_rafdb_ensure_skeleton install_root)
    file(MAKE_DIRECTORY
        "${install_root}/rafdb/task_batch"
        "${install_root}/rafdb/kernel"
        "${install_root}/rafdb/task"
        "${install_root}/rafdb/log"
        "${install_root}/rafdb/runtime_environment")
endfunction()

function(tpb_rafdb_copy_domain build_dir install_root domain rel_dir)
    if(NOT EXISTS "${build_dir}/rafdb/${rel_dir}")
        return()
    endif()
    tpb_rafdb_has_data("${build_dir}/rafdb/${rel_dir}" _src_has)
    if(NOT _src_has)
        return()
    endif()
    file(COPY "${build_dir}/rafdb/${rel_dir}/"
         DESTINATION "${install_root}/rafdb/${rel_dir}/")
    message(STATUS
        "TPBench rafdb sync: copied ${domain} metadata to ${install_root}/rafdb/${rel_dir}")
endfunction()

function(tpb_rafdb_copy_metadata build_dir install_root copy_kernel copy_rtenv merge_config)
    tpb_rafdb_ensure_skeleton("${install_root}")

    if(copy_kernel)
        tpb_rafdb_copy_domain("${build_dir}" "${install_root}" "kernel" "kernel")
    endif()
    if(copy_rtenv)
        tpb_rafdb_copy_domain("${build_dir}" "${install_root}"
            "runtime_environment" "runtime_environment")
    endif()
    if(merge_config AND (copy_kernel OR copy_rtenv))
        tpb_rafdb_merge_config("${build_dir}" "${install_root}")
    endif()
endfunction()

function(tpb_rafdb_backup install_root)
    execute_process(
        COMMAND date +%Y%m%dT%H%M%S
        OUTPUT_VARIABLE _ts
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE _date_rc)
    if(NOT _date_rc EQUAL 0 OR _ts STREQUAL "")
        message(FATAL_ERROR "TPBench rafdb sync: failed to obtain backup timestamp")
    endif()

    set(_bak_parent "${install_root}/rafdb.bak.d")
    set(_bak_dir "${_bak_parent}/rafdb_${_ts}")
    file(MAKE_DIRECTORY "${_bak_parent}")

    if(EXISTS "${install_root}/rafdb")
        file(RENAME "${install_root}/rafdb" "${_bak_dir}")
        message(STATUS "TPBench rafdb sync: backed up existing rafdb to ${_bak_dir}")
    endif()
endfunction()

function(tpb_rafdb_resolve_mode out_var)
    if(DEFINED ENV{TPB_INSTALL_RAFDB} AND NOT "$ENV{TPB_INSTALL_RAFDB}" STREQUAL "")
        string(TOUPPER "$ENV{TPB_INSTALL_RAFDB}" _mode)
    elseif(DEFINED TPB_INSTALL_RAFDB AND NOT TPB_INSTALL_RAFDB STREQUAL "")
        string(TOUPPER "${TPB_INSTALL_RAFDB}" _mode)
    else()
        set(_mode "AUTO")
    endif()
    set(${out_var} "${_mode}" PARENT_SCOPE)
endfunction()

# --- main -------------------------------------------------------------------

tpb_rafdb_resolve_tpb_home(_tpb_home)
get_filename_component(_tpb_home "${_tpb_home}" ABSOLUTE)

tpb_rafdb_resolve_install_root(_install_root)

get_filename_component(_install_prefix_abs "${CMAKE_INSTALL_PREFIX}" ABSOLUTE)

if(NOT _tpb_home STREQUAL _install_prefix_abs)
    message(WARNING
        "TPBench rafdb sync: CMAKE_INSTALL_PREFIX (${_install_prefix_abs}) differs from "
        "TPB_HOME (${_tpb_home}). Use: cmake --install build --prefix \"${_tpb_home}\"")
endif()

set(_sync_root "${_install_root}")

tpb_rafdb_has_data("${_sync_root}/rafdb/kernel" _dst_kernel)
tpb_rafdb_has_data("${_sync_root}/rafdb/runtime_environment" _dst_rtenv)
tpb_rafdb_has_data("${TBP_BUILD_DIR}/rafdb/kernel" _src_kernel)
tpb_rafdb_has_data("${TBP_BUILD_DIR}/rafdb/runtime_environment" _src_rtenv)

if(NOT _src_kernel AND NOT _src_rtenv)
    message(STATUS "TPBench rafdb sync: no build-tree metadata; skipping")
    return()
endif()

set(_copy_kernel FALSE)
set(_copy_rtenv FALSE)
if(_src_kernel AND NOT _dst_kernel)
    set(_copy_kernel TRUE)
endif()
if(_src_rtenv AND NOT _dst_rtenv)
    set(_copy_rtenv TRUE)
endif()

set(_overwrite_kernel FALSE)
if(_src_kernel AND _dst_kernel)
    set(_overwrite_kernel TRUE)
endif()
set(_overwrite_rtenv FALSE)
if(_src_rtenv AND _dst_rtenv)
    set(_overwrite_rtenv TRUE)
endif()
set(_needs_overwrite FALSE)
if(_overwrite_kernel OR _overwrite_rtenv)
    set(_needs_overwrite TRUE)
endif()

if(NOT _copy_kernel AND NOT _copy_rtenv AND NOT _needs_overwrite)
    message(STATUS "TPBench rafdb sync: destination metadata up to date; skipping")
    return()
endif()

tpb_rafdb_resolve_mode(_mode)

set(_do_backup FALSE)

if(_mode STREQUAL "NO")
    message(STATUS "TPBench rafdb sync: TPB_INSTALL_RAFDB=NO; skipping")
    return()
elseif(_mode STREQUAL "YES")
    if(_src_kernel)
        set(_copy_kernel TRUE)
    endif()
    if(_src_rtenv)
        set(_copy_rtenv TRUE)
    endif()
    if(_needs_overwrite)
        set(_do_backup TRUE)
    endif()
elseif(_needs_overwrite)
    set(_warn_domains "")
    if(_overwrite_kernel)
        string(APPEND _warn_domains "kernel")
    endif()
    if(_overwrite_rtenv)
        if(NOT _warn_domains STREQUAL "")
            string(APPEND _warn_domains ", ")
        endif()
        string(APPEND _warn_domains "runtime_environment")
    endif()
    message(WARNING
        "TPBench rafdb sync: metadata already exists under "
        "${_sync_root}/rafdb/${_warn_domains}. "
        "Skipping overwrite (copying only into empty domains). "
        "Set TPB_INSTALL_RAFDB=YES to backup and replace.")
endif()

if(NOT _copy_kernel AND NOT _copy_rtenv)
    message(STATUS "TPBench rafdb sync: nothing to copy; skipping")
    return()
endif()

if(_do_backup)
    tpb_rafdb_backup("${_sync_root}")
endif()

tpb_rafdb_copy_metadata("${TBP_BUILD_DIR}" "${_sync_root}"
    "${_copy_kernel}" "${_copy_rtenv}" TRUE)
message(STATUS "TPBench rafdb sync: complete (${_sync_root}/rafdb)")
