# SPDX-License-Identifier: GPL-2.0+ OR MIT
# Copyright (c) 2026 Frank Secilia
#
# Kernel Driver Build Module
#
# This module serves two distinct purposes:
#   - Symlinks source to build tree and invokes Kbuild for testing local builds.
#   - Installs source and Kbuild to dkms root.

# transforms a list of source files into a list of object files for kbuild
function(src_to_obj result)
    set(objs ${ARGN})
    list(TRANSFORM objs REPLACE "\\.[^.]+$" ".o")
    set(${result} "${objs}" PARENT_SCOPE)
endfunction()

# =====================================================================================================================
# Environment
# =====================================================================================================================

# git hash for versioning
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE git_hash
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT git_hash)
    set(git_hash standalone)
endif()

# kernel version for kbuild location
if("$ENV{KVER}" STREQUAL "")
    execute_process(
        COMMAND uname -r
        OUTPUT_VARIABLE kernel_version
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
else()
    set(kernel_version $ENV{KVER})
endif()
set(kbuild_root /lib/modules/${kernel_version}/build)

# =====================================================================================================================
# File Lists
# =====================================================================================================================
# Files are categorized by:
#   - type: headers vs source, because headers can't go in Kbuild source lists
#   - scope: driver-only, library-only, or shared
#   - fpu: files requiring the fpu need specific kbuild flags
#
# These are kept separate and combined rather than filtered.

# headers
set(hdrs_driver
    fpu_guard.hpp
    math.hpp
)

# shared source files requiring fpu flags
set(srcs_shared_fpu
    math.cpp
)

# shared source files that do not require fpu flags
set(srcs_shared
)

# driver-only source
set(srcs_driver
    entry_point.c
    fpu_guard.cpp
)

# library-only source
set(srcs_lib
    shim/linux/compiler.h
    shim/linux/printk.h
    shim/linux/types.h
)

# =====================================================================================================================
# Build Configuration
# =====================================================================================================================

# kernel-compatible C flags
list(APPEND driver_c_flags ${c_flags}
    -ffreestanding
)

# kernel-compatible C++ flags
list(APPEND driver_cxx_flags ${cxx_flags}
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

# bare object library containing shared files
add_library(driver_lib OBJECT
    ${srcs_shared}
    ${srcs_shared_fpu}
    ${srcs_lib}
    ${hdrs_driver}
)
target_include_directories(driver_lib PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/shim>
)
target_compile_options(driver_lib PRIVATE
    ${driver_c_flags}
    $<$<COMPILE_LANGUAGE:CXX>:${driver_cxx_flags}>
)

# driver entry points with kernel headers for ide nav
add_library(driver OBJECT EXCLUDE_FROM_ALL ${srcs_driver})
target_link_libraries(driver PRIVATE driver_lib)
add_dependencies(driver kbuild)
target_compile_definitions(driver PRIVATE
    __KERNEL__
    MODULE
    KBUILD_MODNAME=${project_name_underscores}
)
target_include_directories(driver PRIVATE
    ${kbuild_root}/include
    ${kbuild_root}/arch/x86/include
    ${kbuild_root}/arch/x86/include/generated
    ${kbuild_root}/include/uapi
    ${kbuild_root}/arch/x86/include/uapi
    ${kbuild_root}/include/generated/uapi
)
target_compile_options(driver PRIVATE
    -include ${kbuild_root}/include/linux/kconfig.h
)

# =====================================================================================================================
# Kbuild
# =====================================================================================================================
# The kernel module is built using Kbuild. The Kbuild file is generated here. All builds require the Kbuild file
# eventually. Packaging builds install it next to the kernel source files, staging the kernel module for dkms. Binary
# builds symlink the source next to it and build there to keep its output in the build tree. Since the Kbuild may be
# part of a shadow build, its is always generated in a shadow subdir.

set(kbuild_shadow_root ${CMAKE_CURRENT_BINARY_DIR}/shadow)

# ---------------------------------------------------------------------------------------------------------------------
# Generated File
# ---------------------------------------------------------------------------------------------------------------------

# extract c++ standard flag from cmake target
get_target_property(driver_lib_std_version driver_lib CXX_STANDARD)
set(driver_lib_std_version_flag ${CMAKE_CXX${driver_lib_std_version}_STANDARD_COMPILE_OPTION})

# format flags for Kbuild
set(kbuild_c_flags ${driver_c_flags})
set(kbuild_cxx_flags ${driver_lib_std_version_flag} ${driver_cxx_flags})
list(JOIN kbuild_c_flags " " kbuild_c_flags)
list(JOIN kbuild_cxx_flags " " kbuild_cxx_flags)

# compose object file lists
src_to_obj(objs_kbuild ${srcs_shared} ${srcs_driver})
src_to_obj(objs_kbuild_float ${srcs_shared_fpu})
list(JOIN objs_kbuild " " objs_kbuild)
list(JOIN objs_kbuild_float " " objs_kbuild_float)

# generate
set(kbuild Kbuild)
set(kbuild_output ${project_name_underscores}.o)
set(kbuild_gen ${kbuild_shadow_root}/${kbuild})
configure_file(${kbuild}.in ${kbuild_gen} @ONLY)

# ---------------------------------------------------------------------------------------------------------------------
# Source Symlinks
# ---------------------------------------------------------------------------------------------------------------------

# collect files to shadow
set(srcs_shadow_rel
    ${hdrs_driver}
    ${srcs_shared}
    ${srcs_shared_fpu}
    ${srcs_driver}
    Makefile
)

foreach(src_rel IN LISTS srcs_shadow_rel)
    # resolve absolute paths
    get_filename_component(src_abs ${src_rel} ABSOLUTE)
    get_filename_component(subdir ${src_rel} DIRECTORY)
    set(dst_dir ${kbuild_shadow_root}/${subdir})
    set(dst ${kbuild_shadow_root}/${src_rel})

    # create symlink
    add_custom_command(
        OUTPUT ${dst}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${dst_dir}
        COMMAND ${CMAKE_COMMAND} -E create_symlink ${src_abs} ${dst}
        DEPENDS ${src_abs}
        COMMENT "Shadowing ${src_rel}"
        VERBATIM
    )
endforeach()

# =====================================================================================================================
# Installation
# =====================================================================================================================

install(FILES ${kbuild_gen} DESTINATION ${dkms_dst_dir} COMPONENT dkms)

# install source files to dkms
foreach(src_rel IN LISTS srcs_shadow_rel)
    get_filename_component(dst_dir "${src_rel}" DIRECTORY)
    install(FILES "${src_rel}" DESTINATION "${dkms_dst_dir}/${dst_dir}" COMPONENT dkms)
endforeach()

# =====================================================================================================================
# Build Targets
# =====================================================================================================================

# ---------------------------------------------------------------------------------------------------------------------
# Build Target
# ---------------------------------------------------------------------------------------------------------------------

# collect all staged files as dependencies
set(srcs_shadow_abs ${kbuild_gen})
foreach(src_rel IN LISTS srcs_shadow_rel)
    list(APPEND srcs_shadow_abs ${kbuild_shadow_root}/${src_rel})
endforeach()

# configure clang builds
set(kbuild_flags "")
if("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    list(APPEND kbuild_flags "LLVM=1")
endif()

# define external make command
set(kbuild_make_command
    make -sC ${kbuild_root} ${kbuild_flags} V=1
    CC="${CMAKE_C_COMPILER}"
    CXX="${CMAKE_CXX_COMPILER}"
    M=${kbuild_shadow_root}
)

# run make modules
add_custom_command(
    OUTPUT ${kbuild_shadow_root}/${kbuild_output}
    COMMAND ${kbuild_make_command} modules
    WORKING_DIRECTORY ${kbuild_shadow_root}
    DEPENDS ${srcs_shadow_abs}
    COMMENT "Building kernel module"
    VERBATIM
    USES_TERMINAL
)

# kick off kbuild
add_custom_target(kbuild DEPENDS ${kbuild_shadow_root}/${kbuild_output})

# ---------------------------------------------------------------------------------------------------------------------
# Clean
# ---------------------------------------------------------------------------------------------------------------------

# run make clean
add_custom_target(clean_kbuild
    COMMAND ${kbuild_make_command} clean
    WORKING_DIRECTORY ${kbuild_shadow_root}
    COMMENT "Cleaning kernel module"
    VERBATIM
    USES_TERMINAL
)
