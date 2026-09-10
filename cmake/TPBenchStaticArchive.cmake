# TPBenchStaticArchive.cmake
# Classify toolchains, link ordinary static archives, and post-link audit
# a kernel DSO load group. Does not copy or promote compiler-rt.

if(NOT DEFINED TPB_CORE_STATIC_LIBS)
    set(TPB_CORE_STATIC_LIBS "" CACHE STRING
        "Extra .a files linked into libtpbench.so (never MPI)")
endif()
if(NOT DEFINED TPB_KERNEL_STATIC_LIBS)
    set(TPB_KERNEL_STATIC_LIBS "" CACHE STRING
        "Extra .a files linked into every CPU kernel .so (ordinary archive). Post-link audit fails the target if the load group has a strong unsatisfied U")
endif()

set(TPB_AUDIT_KERNEL_SCRIPT
    "${CMAKE_CURRENT_LIST_DIR}/TPBenchAuditKernel.cmake")

function(tpb_elf_export_program out_var)
    if(TARGET tpb-elf-export)
        set(${out_var} "$<TARGET_FILE:tpb-elf-export>" PARENT_SCOPE)
        return()
    endif()
    if(DEFINED TPBENCH_ELF_EXPORT AND EXISTS "${TPBENCH_ELF_EXPORT}")
        set(${out_var} "${TPBENCH_ELF_EXPORT}" PARENT_SCOPE)
        return()
    endif()
    find_program(_tpb_elfexp NAMES tpb-elf-export)
    if(_tpb_elfexp)
        set(${out_var} "${_tpb_elfexp}" PARENT_SCOPE)
        return()
    endif()
    set(${out_var} "" PARENT_SCOPE)
endfunction()

function(tpb_classify_compiler cc out_var)
    set(_fam "gcc")
    if("${cc}" STREQUAL "")
        set(${out_var} "${_fam}" PARENT_SCOPE)
        return()
    endif()
    get_filename_component(_ccname "${cc}" NAME)
    set(_show "")
    execute_process(COMMAND "${cc}" -showme
                    OUTPUT_VARIABLE _show
                    ERROR_QUIET
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
    if("${_show}" STREQUAL "")
        execute_process(COMMAND "${cc}" -show
                        OUTPUT_VARIABLE _show
                        ERROR_QUIET
                        OUTPUT_STRIP_TRAILING_WHITESPACE)
    endif()
    string(TOLOWER "${_ccname} ${_show}" _blob)
    # Do not treat "mpicc" as Intel: the old "icc" substring matched inside it.
    if(_ccname MATCHES "^(mpi)?icx$" OR _ccname MATCHES "^(mpi)?icc$"
       OR _blob MATCHES "oneapi")
        set(_fam "intel")
    elseif(_show MATCHES "rtlib=compiler-rt")
        set(_fam "clang_rt")
    elseif(_blob MATCHES "clang" OR _ccname MATCHES "clang")
        execute_process(COMMAND "${cc}" -rtlib=compiler-rt -print-libgcc-file-name
                        OUTPUT_VARIABLE _rt
                        ERROR_QUIET
                        OUTPUT_STRIP_TRAILING_WHITESPACE)
        if(_rt MATCHES "clang_rt.builtins")
            set(_fam "clang_rt")
        endif()
    endif()
    set(${out_var} "${_fam}" PARENT_SCOPE)
endfunction()

function(tpb_imported_location tgt out_var)
    set(_loc "")
    if(NOT TARGET "${tgt}")
        set(${out_var} "" PARENT_SCOPE)
        return()
    endif()
    get_target_property(_type "${tgt}" TYPE)
    if(_type STREQUAL "SHARED_LIBRARY" OR _type STREQUAL "UNKNOWN"
       OR _type STREQUAL "UNKNOWN_LIBRARY" OR _type STREQUAL "INTERFACE_LIBRARY")
        foreach(_c LOCATION LOCATION_RELEASE LOCATION_RELWITHDEBINFO
                    LOCATION_MINSIZEREL LOCATION_DEBUG
                    IMPORTED_LOCATION IMPORTED_LOCATION_RELEASE
                    IMPORTED_LOCATION_RELWITHDEBINFO
                    IMPORTED_LOCATION_MINSIZEREL IMPORTED_LOCATION_DEBUG)
            get_target_property(_try "${tgt}" ${_c})
            if(_try AND EXISTS "${_try}")
                set(_loc "${_try}")
                break()
            endif()
        endforeach()
    endif()
    set(${out_var} "${_loc}" PARENT_SCOPE)
endfunction()

function(tpb_classify_dso path out_var)
    set(_fam "gcc")
    if(NOT EXISTS "${path}")
        set(${out_var} "${_fam}" PARENT_SCOPE)
        return()
    endif()
    tpb_elf_export_program(_tool)
    if(NOT _tool STREQUAL "" AND NOT _tool MATCHES "TARGET_FILE" AND EXISTS "${_tool}")
        execute_process(COMMAND "${_tool}" classify-dso "${path}"
                        OUTPUT_VARIABLE _out
                        ERROR_QUIET
                        RESULT_VARIABLE _rc
                        OUTPUT_STRIP_TRAILING_WHITESPACE)
        if(_rc EQUAL 0 AND NOT "${_out}" STREQUAL "")
            set(${out_var} "${_out}" PARENT_SCOPE)
            return()
        endif()
    endif()
    execute_process(COMMAND readelf -p .comment "${path}"
                    OUTPUT_VARIABLE _cmt
                    ERROR_QUIET)
    execute_process(COMMAND readelf -d "${path}"
                    OUTPUT_VARIABLE _dyn
                    ERROR_QUIET)
    if(_cmt MATCHES "Intel" OR _cmt MATCHES "icc")
        set(_fam "intel")
    elseif(_cmt MATCHES "clang" OR _cmt MATCHES "AOCC")
        set(_fam "clang_rt")
    elseif(_dyn MATCHES "libirc" OR _dyn MATCHES "libimf")
        set(_fam "intel")
    endif()
    set(${out_var} "${_fam}" PARENT_SCOPE)
endfunction()

function(tpb_reject_mpi_static_libs libs)
    foreach(_a ${libs})
        string(TOLOWER "${_a}" _al)
        if(_al MATCHES "mpi" OR _al MATCHES "libmpi")
            message(FATAL_ERROR
                "TPB_CORE_STATIC_LIBS must not contain MPI ('${_a}'). "
                "libtpbench.so stays MPI-free.")
        endif()
    endforeach()
endfunction()

function(tpb_require_same_toolchain kernel_cc dep_paths)
    tpb_classify_compiler("${kernel_cc}" _kfam)
    foreach(_p ${dep_paths})
        if(NOT EXISTS "${_p}")
            continue()
        endif()
        get_filename_component(_bn "${_p}" NAME)
        if(_bn MATCHES "^libclang_rt" OR _bn MATCHES "compiler-rt")
            continue()
        endif()
        tpb_classify_dso("${_p}" _dfam)
        if(NOT "${_dfam}" STREQUAL "${_kfam}")
            message(FATAL_ERROR
                "kernel toolchain ${_kfam}, dependency ${_p} toolchain ${_dfam}; "
                "rebuild the kernel with a matching compiler or use libraries "
                "built with the same toolchain family (gcc / clang_rt / intel).")
        endif()
    endforeach()
endfunction()

function(tpb_resolve_link_dsos link_libs out_var)
    set(_dsos "")
    foreach(_l ${link_libs})
        if(TARGET "${_l}")
            tpb_imported_location("${_l}" _loc)
            if(_loc)
                list(APPEND _dsos "${_loc}")
            endif()
            get_target_property(_iface "${_l}" INTERFACE_LINK_LIBRARIES)
            if(_iface)
                foreach(_il ${_iface})
                    if(TARGET "${_il}")
                        tpb_imported_location("${_il}" _iloc)
                        if(_iloc)
                            list(APPEND _dsos "${_iloc}")
                        endif()
                    elseif(EXISTS "${_il}" AND _il MATCHES "\\.(so|dylib)($|\\.)")
                        list(APPEND _dsos "${_il}")
                    endif()
                endforeach()
            endif()
        elseif(EXISTS "${_l}" AND _l MATCHES "\\.(so|dylib)($|\\.)")
            list(APPEND _dsos "${_l}")
        endif()
    endforeach()
    set(${out_var} "${_dsos}" PARENT_SCOPE)
endfunction()

function(tpb_audit_kernel_dso tgt)
    if(APPLE)
        return()
    endif()
    tpb_elf_export_program(_tool)
    if(_tool STREQUAL "")
        message(FATAL_ERROR
            "tpb-elf-export not found; reinstall TPBench or build "
            "the tpb-elf-export target.")
    endif()
    if(NOT EXISTS "${TPB_AUDIT_KERNEL_SCRIPT}")
        message(FATAL_ERROR
            "TPBenchAuditKernel.cmake not found at ${TPB_AUDIT_KERNEL_SCRIPT}")
    endif()
    add_custom_command(TARGET ${tgt} POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            "-DTPB_ELF_EXPORT=${_tool}"
            "-DTPB_KERNEL_SO=$<TARGET_FILE:${tgt}>"
            -P "${TPB_AUDIT_KERNEL_SCRIPT}"
        COMMENT "Audit load-group symbols in ${tgt}"
        VERBATIM)
    if(TARGET tpb-elf-export)
        add_dependencies(${tgt} tpb-elf-export)
    endif()
endfunction()

function(tpb_apply_kernel_runtimes tgt kernel_cc extra_static extra_link)
    set(_statics "")
    if(NOT "${extra_static}" STREQUAL "")
        separate_arguments(_ex UNIX_COMMAND "${extra_static}")
        list(APPEND _statics ${_ex})
    endif()
    if(NOT "${TPB_KERNEL_STATIC_LIBS}" STREQUAL "")
        separate_arguments(_ks UNIX_COMMAND "${TPB_KERNEL_STATIC_LIBS}")
        list(APPEND _statics ${_ks})
    endif()
    list(REMOVE_DUPLICATES _statics)

    tpb_resolve_link_dsos("${extra_link}" _dsos)
    tpb_require_same_toolchain("${kernel_cc}" "${_dsos}")

    if(_statics)
        tpb_classify_compiler("${kernel_cc}" _kfam)
        foreach(_a ${_statics})
            if(_a STREQUAL "")
                continue()
            endif()
            if(NOT EXISTS "${_a}")
                message(FATAL_ERROR "Static archive not found: ${_a}")
            endif()
            if(_a MATCHES "clang_rt" AND NOT _kfam STREQUAL "clang_rt")
                message(FATAL_ERROR
                    "Static archive '${_a}' is clang_rt but kernel toolchain "
                    "is ${_kfam}. Rebuild with a matching compiler.")
            endif()
            target_link_libraries(${tgt} PRIVATE "${_a}")
        endforeach()
    endif()
    tpb_audit_kernel_dso(${tgt})
endfunction()

function(tpb_apply_core_runtimes tgt host_cc)
    tpb_reject_mpi_static_libs("${TPB_CORE_STATIC_LIBS}")
    set(_statics "")
    if(NOT "${TPB_CORE_STATIC_LIBS}" STREQUAL "")
        separate_arguments(_cs UNIX_COMMAND "${TPB_CORE_STATIC_LIBS}")
        tpb_reject_mpi_static_libs("${_cs}")
        list(APPEND _statics ${_cs})
    endif()
    list(REMOVE_DUPLICATES _statics)
    if(_statics)
        foreach(_a ${_statics})
            if(NOT EXISTS "${_a}")
                message(FATAL_ERROR "Static archive not found: ${_a}")
            endif()
            target_link_libraries(${tgt} PRIVATE "${_a}")
        endforeach()
    endif()
endfunction()
