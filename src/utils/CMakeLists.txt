# Host tools used by kernel post-link audit (Linux ELF only).

if(NOT APPLE)
    add_executable(tpb-elf-export tpb-elf-export.c)
    target_compile_options(tpb-elf-export PRIVATE -O2)
    set_target_properties(tpb-elf-export PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
    install(TARGETS tpb-elf-export
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
