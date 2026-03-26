# Relocatable RPATH for targets that link libtpbench (install layout: bin/ + lib/).
# Exe and .tpbx under bin/ need ../lib; kernel .so next to libtpbench.so needs $ORIGIN only.

function(tpb_set_install_rpath_tpbench_exe target)
    set_target_properties(${target} PROPERTIES
        BUILD_RPATH "\$ORIGIN/../lib"
        INSTALL_RPATH "\$ORIGIN/../lib")
endfunction()

function(tpb_set_install_rpath_tpbench_shlib target)
    set_target_properties(${target} PROPERTIES
        BUILD_RPATH "\$ORIGIN"
        INSTALL_RPATH "\$ORIGIN")
endfunction()
