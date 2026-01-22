# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frank Secilia
#
# This file manages .deb packaging. It configures the deb-specific metadata and scripts, then configures a cmake script
# run dpkg-deb.

set(deb_gen_root ${CMAKE_CURRENT_BINARY_DIR}/debian)

# ---------------------------------------------------------------------------------------------------------------------
# Generated Files
# ---------------------------------------------------------------------------------------------------------------------

# CMake chokes on these strings, even in @ONLY mode, so our only recourse is to substitute them explicitly.
set(deb_shlibs_depends "\${shlibs:Depends}")
set(deb_misc_depends   "\${misc:Depends}")

set(deb_control control)
set(deb_controls_gen ${deb_gen_root}/${deb_control})
configure_file(${deb_control}.in ${deb_control_gen} @ONLY USE_SOURCE_PERMISSIONS)

set(deb_postinst postinst)
set(deb_postinst_gen ${deb_gen_root}/${project_name_dashes}.${deb_postinst})
configure_file(${deb_postinst}.in ${deb_postinst_gen} @ONLY USE_SOURCE_PERMISSIONS)

set(deb_prerm prerm)
set(deb_prerm_gen ${deb_gen_root}/${project_name_dashes}.${deb_prerm})
configure_file(${deb_prerm}.in ${deb_prerm_gen} @ONLY USE_SOURCE_PERMISSIONS)

# ---------------------------------------------------------------------------------------------------------------------
# Installation
# ---------------------------------------------------------------------------------------------------------------------

set(deb_cmake deb.cmake)
set(deb_cmake_gen ${deb_gen_root}/${deb_cmake})
configure_file(${deb_cmake}.in ${deb_cmake_gen} @ONLY)
add_custom_target(deb
    COMMAND ${CMAKE_COMMAND}
        -D CONFIG="$<CONFIG>"
        -P ${deb_cmake_gen}
    COMMENT "Building .deb package"
    USES_TERMINAL
)
