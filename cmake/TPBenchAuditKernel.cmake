# TPBenchAuditKernel.cmake
# Post-link load-group audit. Deletes the kernel .so on failure (Ninja-safe).
#
# cmake -D TPB_ELF_EXPORT=<tool> -D TPB_KERNEL_SO=<lib.so> -P this-file

if(NOT TPB_ELF_EXPORT)
    message(FATAL_ERROR "TPBenchAuditKernel: TPB_ELF_EXPORT is not set")
endif()
if(NOT TPB_KERNEL_SO)
    message(FATAL_ERROR "TPBenchAuditKernel: TPB_KERNEL_SO is not set")
endif()

execute_process(
    COMMAND "${TPB_ELF_EXPORT}" audit-needed "${TPB_KERNEL_SO}"
    RESULT_VARIABLE _tpb_audit_rc)
if(NOT _tpb_audit_rc EQUAL 0)
    execute_process(COMMAND "${CMAKE_COMMAND}" -E rm -f "${TPB_KERNEL_SO}")
    message(FATAL_ERROR
        "TPBench: load-group audit failed for ${TPB_KERNEL_SO} "
        "(exit ${_tpb_audit_rc}). The .so was deleted. "
        "Recompile the dependency so it provides the missing symbols.")
endif()
