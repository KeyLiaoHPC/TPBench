if(NOT DEFINED OUT)
    message(FATAL_ERROR "OUT not set for flattened tpbench.h output.")
endif()
if(NOT DEFINED TPB_ROOT)
    message(FATAL_ERROR "TPB_ROOT not set for source root.")
endif()

set(_tpb_headers
    "${TPB_ROOT}/src/tpb-unitdefs.h"
    "${TPB_ROOT}/src/tpb-types.h"
    "${TPB_ROOT}/src/tpb-driver.h"
    "${TPB_ROOT}/src/tpb-io.h"
)

get_filename_component(_out_dir "${OUT}" DIRECTORY)
file(MAKE_DIRECTORY "${_out_dir}")

file(WRITE "${OUT}"
"/* Auto-generated: flattened public header for TPBench. */\n"
"#ifndef TPBENCH_H\n"
"#define TPBENCH_H\n"
"\n")

foreach(_hdr IN LISTS _tpb_headers)
    if(NOT EXISTS "${_hdr}")
        message(FATAL_ERROR "Missing header: ${_hdr}")
    endif()

    file(READ "${_hdr}" _content)
    string(REPLACE "\r" "" _content "${_content}")
    string(REPLACE "\n" ";" _lines "${_content}")

    file(APPEND "${OUT}" "/* BEGIN ${_hdr} */\n")
    foreach(_line IN LISTS _lines)
        if(_line MATCHES "^[ \t]*#include[ \t]+\"tpb-(types|driver|io|unitdefs)\\.h\"")
            continue()
        endif()
        file(APPEND "${OUT}" "${_line}\n")
    endforeach()
    file(APPEND "${OUT}" "/* END ${_hdr} */\n\n")
endforeach()

file(APPEND "${OUT}" "#endif /* TPBENCH_H */\n")
