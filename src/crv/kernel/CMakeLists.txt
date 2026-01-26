# SPDX-License-Identifier: GPL-2.0+ OR MIT
# Copyright (c) 2026 Frank Secilia
#
# kernel module build
#
# This file serves these purposes:
#  - provides a user-mode library that shims the kernel headers
#  - collects files listed in the global kernel manifest
#  - generates the Kbuild necessary to build the kernel module
#  - manages the kernel module cmake install to stage dkms for packaging
#  - symlinks a shadow build in the build tree to build the kernel module locally, directly

# =====================================================================================================================
# Environment
# =====================================================================================================================
# detect external paths and versions needed by both user-mode targets and Kbuild

# git hash for version string
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

# kernel headers location
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

# shadow build location
set(kbuild_shadow_root "${CMAKE_BINARY_DIR}/shadow")

# =====================================================================================================================
# Source Files
# =====================================================================================================================
# collect source from two places:
#   - global kernel_manifest, populated by target_sources_kernel() calls throughout the tree
#   - local, kernel-only files that aren't part of any user-mode libraries

# ---------------------------------------------------------------------------------------------------------------------
# Kernel-Only Files
# ---------------------------------------------------------------------------------------------------------------------
# these exist only for the kernel module and aren't shared with user-mode code

set(hdrs_kernel
    input/handler.h
)

set(srcs_kernel
    input/handler.c
    entry_point.c
)

# ---------------------------------------------------------------------------------------------------------------------
# Shared Files from Manifest
# ---------------------------------------------------------------------------------------------------------------------
# these were registered elsewhere via target_sources_kernel() and compile into both the kernel module and user-mode
# libraries

get_property(manifest_files GLOBAL PROPERTY kernel_manifest)

set(hdrs_shared_abs ${manifest_files})
list(FILTER hdrs_shared_abs INCLUDE REGEX "\\.(h|hpp)$")

set(srcs_shared_abs ${manifest_files})
list(FILTER srcs_shared_abs INCLUDE REGEX "\\.(c|cpp)$")

# ---------------------------------------------------------------------------------------------------------------------
# Shim Headers
# ---------------------------------------------------------------------------------------------------------------------
# these provide kernel-like types and macros for user-mode builds

set(srcs_shim
    shim/linux/compiler.h
    shim/linux/printk.h
    shim/linux/types.h
)

# =====================================================================================================================
# Compiler Flags
# =====================================================================================================================
# flags that make user-mode compilation kernel-compatible
#
# These apply to user_mode_kernel_shim and are also forwarded to the Kbuild for c++ files.

set(kernel_c_flags
    -ffreestanding
)

set(kernel_cxx_flags
    ${compile_options_common}
    -fno-builtin
    -fno-exceptions
    -fno-rtti
    -fno-threadsafe-statics
    -fno-use-cxa-atexit
    -nostdinc++
    -nostdlib
)

# =====================================================================================================================
# User-Mode Targets
# =====================================================================================================================

# ---------------------------------------------------------------------------------------------------------------------
# user_mode_kernel_shim
# ---------------------------------------------------------------------------------------------------------------------
# shims kernel headers and provides kernel-compatible flags to user-mode targets

add_library(user_mode_kernel_shim OBJECT ${srcs_shim})
set_target_properties(user_mode_kernel_shim PROPERTIES LINKER_LANGUAGE CXX)
target_include_directories(user_mode_kernel_shim PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/shim>"
)

# ---------------------------------------------------------------------------------------------------------------------
# kernel_nav
# ---------------------------------------------------------------------------------------------------------------------
# fake target for kernel-only files to provide ide navigation
#
# This target uses the real kernel headers. It is never actually linked into anything.

add_library(kernel_nav OBJECT EXCLUDE_FROM_ALL ${hdrs_kernel} ${srcs_kernel})
target_link_libraries(kernel_nav PRIVATE user_mode_kernel_shim)
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
# manage Kbuilds to build kernel module
#
# The kernel module is built using Kbuild. We either kick it off locally using a shadow build in the build tree, or
# package it for dkms using cmake install.

# creates shadow build symlink and registers file for cmake install
macro(add_to_kbuild src_abs)
    # find paths relative to project source root
    file(RELATIVE_PATH _src_rel "${PROJECT_SOURCE_DIR}/src" "${src_abs}")
    get_filename_component(_subdir "${_src_rel}" DIRECTORY)
    set(_dst_dir "${kbuild_shadow_root}/${_subdir}")
    set(_dst "${kbuild_shadow_root}/${_src_rel}")

    # track for Kbuild and dependencies
    list(APPEND srcs_kbuild_rel "${_src_rel}")
    list(APPEND srcs_kbuild_abs "${_dst}")

    # symlink to shadow build
    add_custom_command(
        OUTPUT "${_dst}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${_dst_dir}"
        COMMAND "${CMAKE_COMMAND}" -E create_symlink "${src_abs}" "${_dst}"
        DEPENDS "${src_abs}"
        COMMENT "shadowing ${_src_rel}"
        VERBATIM
    )

    # install to dkms tree
    install(FILES "${src_abs}" DESTINATION "${dkms_dst_dir}/${_subdir}" COMPONENT dkms)
endmacro()

# ---------------------------------------------------------------------------------------------------------------------
# Shadow Build
# ---------------------------------------------------------------------------------------------------------------------
# build symlinked kernel module source in build tree
#
# Kbuild expects to build in the source tree. To maintain hygiene, we symlink the kernel module source into
# kbuild_shadow_root and build from there. This way, the real source stays in place and build artifacts land in the
# build tree.

# shadow shared files (already absolute)
foreach(src_abs IN LISTS hdrs_shared_abs srcs_shared_abs)
    add_to_kbuild("${src_abs}")
endforeach()

# shadow kernel-only files (relative, need conversion)
foreach(src_rel IN LISTS hdrs_kernel srcs_kernel)
    get_filename_component(src_abs "${src_rel}" ABSOLUTE)
    add_to_kbuild("${src_abs}")
endforeach()

# shadow the standalone Makefile
set(makefile_src "${CMAKE_CURRENT_SOURCE_DIR}/Makefile")
set(makefile_dst "${kbuild_shadow_root}/Makefile")
add_custom_command(
    OUTPUT "${makefile_dst}"
    COMMAND "${CMAKE_COMMAND}" -E create_symlink "${makefile_src}" "${makefile_dst}"
    DEPENDS "${makefile_src}"
    COMMENT "shadowing Makefile"
    VERBATIM
)
install(FILES "Makefile" DESTINATION "${dkms_dst_dir}" COMPONENT dkms)

# ---------------------------------------------------------------------------------------------------------------------
# Kbuild Generation
# ---------------------------------------------------------------------------------------------------------------------
# generate the actual Kbuild file

# extract c++ standard from cmake
get_target_property(user_mode_kernel_shim_std_version user_mode_kernel_shim CXX_STANDARD)
set(user_mode_kernel_shim_std_version_flag "${CMAKE_CXX${user_mode_kernel_shim_std_version}_STANDARD_COMPILE_OPTION}")

# format flags for Kbuild template
set(kbuild_c_flags ${kernel_c_flags})
set(kbuild_cxx_flags ${user_mode_kernel_shim_std_version_flag} ${kernel_cxx_flags})
list(JOIN kbuild_c_flags " " kbuild_c_flags)
list(JOIN kbuild_cxx_flags " " kbuild_cxx_flags)

# convert source list to object list
set(objs_kbuild ${srcs_kbuild_rel})
list(FILTER objs_kbuild EXCLUDE REGEX "\\.(h|hpp)$") # filter headers
list(TRANSFORM objs_kbuild REPLACE "\\.[^.]+$" ".o") # replace extension
list(JOIN objs_kbuild " " objs_kbuild)               # delimit with space instead of ;

# generate file
set(kbuild_output "${project_name_underscores}.o")
set(kbuild_gen "${kbuild_shadow_root}/Kbuild")
configure_file("Kbuild.in" "${kbuild_gen}" @ONLY)
install(FILES "${kbuild_gen}" DESTINATION "${dkms_dst_dir}" COMPONENT dkms)

# ---------------------------------------------------------------------------------------------------------------------
# Build Targets
# ---------------------------------------------------------------------------------------------------------------------

# clang kernel builds need LLVM=1
set(kbuild_flags "")
if("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    list(APPEND kbuild_flags "LLVM=1")
endif()

# encapsulate external make command
set(kbuild_make_command
    make -sC "${kbuild_root}" ${kbuild_flags} V=1
    "CC=${CMAKE_C_COMPILER}"
    "CXX=${CMAKE_CXX_COMPILER}"
    "M=${kbuild_shadow_root}"
)

# main build
add_custom_command(
    OUTPUT "${kbuild_shadow_root}/${kbuild_output}"
    COMMAND ${kbuild_make_command} modules
    WORKING_DIRECTORY "${kbuild_shadow_root}"
    DEPENDS ${srcs_kbuild_abs} "${makefile_dst}" "${kbuild_gen}"
    COMMENT "building kernel module"
    VERBATIM
    USES_TERMINAL
)
add_custom_target(kbuild DEPENDS "${kbuild_shadow_root}/${kbuild_output}")

# clean
add_custom_target(clean_kbuild
    COMMAND ${kbuild_make_command} clean
    WORKING_DIRECTORY "${kbuild_shadow_root}"
    COMMENT "cleaning kernel module"
    VERBATIM
    USES_TERMINAL
)
