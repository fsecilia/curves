# SPDX-License-Identifier: GPL-2.0+ OR MIT
# Copyright (c) 2026 Frank Secilia
#
# kernel module build
#
# This file serves two distinct purposes:
#   - Symlinks source to build tree and invokes Kbuild for testing local builds.
#   - Installs source and Kbuild to dkms root.

# =====================================================================================================================
# Environment
# =====================================================================================================================

# find git hash for versioning
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    OUTPUT_VARIABLE git_hash
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT git_hash)
    set(git_hash standalone)
endif()

# find kernel version for kbuild location
if("$ENV{KVER}" STREQUAL "")
    execute_process(
        COMMAND uname -r
        OUTPUT_VARIABLE kernel_version
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
else()
    set(kernel_version "$ENV{KVER}")
endif()
set(kbuild_root "/lib/modules/${kernel_version}/build")

# =====================================================================================================================
# File Lists
# =====================================================================================================================
# Files are categorized by:
#   - type: headers vs source, because headers can't go in Kbuild source lists
#   - scope: kernel-only, library-only, or shared

get_property(manifest_files GLOBAL PROPERTY kernel_manifest)

# kernel-only headers must be symlinked
set(hdrs_kernel
)

# kernel-only source must be symlinked and passed to kbuild
set(srcs_kernel
    entry_point.c
)

# shared headers must be symlinked and included in user-mode library
set(hdrs_shared ${manifest_files})
list(FILTER hdrs_shared INCLUDE REGEX "\\.(h|hpp)$")

# shared source must be symlinked, passed to kbuild, and included in user-mode library
set(srcs_shared ${manifest_files})
list(FILTER srcs_shared INCLUDE REGEX "\\.(c|cpp)$")

# =====================================================================================================================
# Build Configuration
# =====================================================================================================================

# kernel-compatible c flags
list(APPEND kernel_c_flags
    -ffreestanding
)

# kernel-compatible c++ flags
list(APPEND kernel_cxx_flags ${compile_options_common}
    -fno-builtin
    -fno-exceptions
    -fno-rtti
    -fno-threadsafe-statics
    -fno-use-cxa-atexit
    -nostdinc++
    -nostdlib
)

# =====================================================================================================================
# Build Targets
# =====================================================================================================================

# bare user-mode object library containing shared source
add_library(kernel_lib OBJECT
    ${srcs_shared}
    ${hdrs_shared}

    shim/linux/compiler.h
    shim/linux/printk.h
    shim/linux/types.h
)
set_target_properties(kernel_lib PROPERTIES LINKER_LANGUAGE CXX)
target_include_directories(kernel_lib PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/shim>"
)
target_compile_options(kernel_lib PRIVATE
    ${kernel_c_flags}
    $<$<COMPILE_LANGUAGE:CXX>:${kernel_cxx_flags}>
)

# fake library for kernel-only source and headers to support ide nav
add_library(kernel_nav OBJECT EXCLUDE_FROM_ALL ${hdrs_kernel} ${srcs_kernel})
target_link_libraries(kernel_nav PRIVATE kernel_lib)
add_dependencies(kernel_nav kbuild)
target_compile_definitions(kernel_nav PRIVATE
    __KERNEL__
    MODULE
    "KBUILD_MODNAME=${project_name_underscores}"
)
target_include_directories(kernel_nav PRIVATE
    "${kbuild_root}/include"
    "${kbuild_root}/arch/x86/include"
    "${kbuild_root}/arch/x86/include/generated"
    "${kbuild_root}/include/uapi"
    "${kbuild_root}/arch/x86/include/uapi"
    "${kbuild_root}/include/generated/uapi"
)
target_compile_options(kernel_nav PRIVATE
    -include "${kbuild_root}/include/linux/kconfig.h"
)

# =====================================================================================================================
# Kbuild
# =====================================================================================================================
# The kernel module is built using Kbuild. The Kbuild file is generated here. All builds require the Kbuild file
# eventually. Packaging builds install it next to the kernel source files, staging the kernel module for dkms. Binary
# builds symlink the source next to it and build there to keep its output in the build tree. Since the Kbuild may be
# part of a shadow build, its is always generated in a shadow subdir.

set(kbuild_shadow_root "${CMAKE_CURRENT_BINARY_DIR}/shadow")

# ---------------------------------------------------------------------------------------------------------------------
# Generated File
# ---------------------------------------------------------------------------------------------------------------------

# extract c++ standard flag from cmake target
get_target_property(kernel_lib_std_version kernel_lib CXX_STANDARD)
set(kernel_lib_std_version_flag "${CMAKE_CXX${kernel_lib_std_version}_STANDARD_COMPILE_OPTION}")

# format flags for Kbuild
set(kbuild_c_flags ${kernel_c_flags})
set(kbuild_cxx_flags ${kernel_lib_std_version_flag} ${kernel_cxx_flags})
list(JOIN kbuild_c_flags " " kbuild_c_flags)
list(JOIN kbuild_cxx_flags " " kbuild_cxx_flags)

# compose object file lists
set(objs_kbuild ${srcs_kernel} ${srcs_shared})
list(TRANSFORM objs_kbuild REPLACE "\\.[^.]+$" ".o")
list(JOIN objs_kbuild " " objs_kbuild)

# generate and install
set(kbuild "Kbuild")
set(kbuild_output "${project_name_underscores}.o")
set(kbuild_gen "${kbuild_shadow_root}/${kbuild}")
configure_file("${kbuild}.in" "${kbuild_gen}" @ONLY)
install(FILES "${kbuild_gen}" DESTINATION "${dkms_dst_dir}" COMPONENT dkms)

# ---------------------------------------------------------------------------------------------------------------------
# Source Symlinks
# ---------------------------------------------------------------------------------------------------------------------

# collect files to shadow
set(srcs_shadow_rel
    ${hdrs_kernel}
    ${srcs_kernel}
    ${hdrs_shared}
    ${srcs_shared}
    Makefile
)

# symlink each, collect result paths, and spec install
set(srcs_shadow_abs "${kbuild_gen}")
foreach(src_rel IN LISTS srcs_shadow_rel)
    # resolve absolute paths
    get_filename_component(src_abs "${src_rel}" ABSOLUTE)
    get_filename_component(subdir "${src_rel}" DIRECTORY)
    set(dst_dir "${kbuild_shadow_root}/${subdir}")
    set(dst "${kbuild_shadow_root}/${src_rel}")
    list(APPEND srcs_shadow_abs "${dst}")

    # create symlink
    add_custom_command(
        OUTPUT "${dst}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${dst_dir}"
        COMMAND "${CMAKE_COMMAND}" -E create_symlink "${src_abs}" "${dst}"
        DEPENDS "${src_abs}"
        COMMENT "shadowing ${src_rel}"
        VERBATIM
    )

    # install to dkms
    get_filename_component(dst_dir "${src_rel}" DIRECTORY)
    install(FILES "${src_rel}" DESTINATION "${dkms_dst_dir}/${dst_dir}" COMPONENT dkms)
endforeach()

# ---------------------------------------------------------------------------------------------------------------------
# Build Target
# ---------------------------------------------------------------------------------------------------------------------

# configure clang builds
set(kbuild_flags "")
if("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    list(APPEND kbuild_flags "LLVM=1")
endif()

# define external make command
set(kbuild_make_command
    make -sC "${kbuild_root}" ${kbuild_flags} V=1
    "CC=${CMAKE_C_COMPILER}"
    "CXX=${CMAKE_CXX_COMPILER}"
    "M=${kbuild_shadow_root}"
)

# run make modules
add_custom_command(
    OUTPUT "${kbuild_shadow_root}/${kbuild_output}"
    COMMAND ${kbuild_make_command} modules
    WORKING_DIRECTORY "${kbuild_shadow_root}"
    DEPENDS "${srcs_shadow_abs}"
    COMMENT "building kernel module"
    VERBATIM
    USES_TERMINAL
)

# kick off kbuild
add_custom_target(kbuild DEPENDS "${kbuild_shadow_root}/${kbuild_output}")

# ---------------------------------------------------------------------------------------------------------------------
# Clean
# ---------------------------------------------------------------------------------------------------------------------

# run make clean
add_custom_target(clean_kbuild
    COMMAND ${kbuild_make_command} clean
    WORKING_DIRECTORY "${kbuild_shadow_root}"
    COMMENT "cleaning kernel module"
    VERBATIM
    USES_TERMINAL
)
