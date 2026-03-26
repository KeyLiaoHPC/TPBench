# TPBenchKernelSelect.cmake
#
# Shared kernel name/tag selection (union with TPB_KERNELS and TPB_KERNEL_TAGS).

function(tpb_kernel_should_build kname default_tags_csv sel_names sel_tags out_var)
    string(TOUPPER "${kname}" _ku)
    if(DEFINED TPB_KERNEL_${_ku}_TAGS)
        string(REPLACE "," ";" _ktags "${TPB_KERNEL_${_ku}_TAGS}")
    else()
        string(REPLACE "," ";" _ktags "${default_tags_csv}")
    endif()

    set(_build OFF)
    if("all" IN_LIST sel_names)
        set(_build ON)
    elseif("${kname}" IN_LIST sel_names)
        set(_build ON)
    elseif("default" IN_LIST sel_names AND "default" IN_LIST _ktags)
        set(_build ON)
    endif()

    if(NOT _build AND NOT "${sel_tags}" STREQUAL "")
        foreach(_tag IN LISTS sel_tags)
            if("${_tag}" IN_LIST _ktags)
                set(_build ON)
                break()
            endif()
        endforeach()
    endif()

    set(${out_var} "${_build}" PARENT_SCOPE)
endfunction()
