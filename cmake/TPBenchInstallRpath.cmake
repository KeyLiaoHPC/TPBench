# Relocatable RPATH for targets that link libtpbench (install layout: bin/ + lib/).
# Exe under bin/ need ../lib; kernel shlib next to libtpbench needs sibling lookup.

function(tpb_set_install_rpath_tpbench_exe target)
    if(APPLE)
        set_target_properties(${target} PROPERTIES
            BUILD_RPATH "@loader_path/../lib"
            INSTALL_RPATH "@loader_path/../lib")
    else()
        set_target_properties(${target} PROPERTIES
            BUILD_RPATH "\$ORIGIN/../lib"
            INSTALL_RPATH "\$ORIGIN/../lib")
    endif()
endfunction()

function(tpb_set_install_rpath_tpbench_shlib target)
    if(APPLE)
        set_target_properties(${target} PROPERTIES
            BUILD_RPATH "@loader_path"
            INSTALL_RPATH "@loader_path")
    else()
        set_target_properties(${target} PROPERTIES
            BUILD_RPATH "\$ORIGIN"
            INSTALL_RPATH "\$ORIGIN")
    endif()
endfunction()
