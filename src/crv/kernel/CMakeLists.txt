# SPDX-License-Identifier: GPL-2.0+ OR MIT
# Copyright (c) 2026 Frank Secilia
#
# kernel module build
#
# This file serves two distinct purposes:
#   - Symlinks source to build tree and invokes Kbuild for testing local builds.
#   - Installs source and Kbuild to dkms root.

# =====================================================================================================================
# File Lists
# =====================================================================================================================
# Files are categorized by:
#   - type: headers vs source, because headers can't go in Kbuild source lists
#   - scope: kernel-only, library-only, or shared

get_property(manifest_files GLOBAL PROPERTY kernel_manifest)

# kernel-only headers must be symlinked
set(hdrs_kernel
    input/handler.h
)

# kernel-only source must be symlinked and passed to kbuild
set(srcs_kernel
    input/handler.c
    entry_point.c
)

# shared headers must be symlinked and included in user-mode library
set(hdrs_shared_abs ${manifest_files})
list(FILTER hdrs_shared_abs INCLUDE REGEX "\\.(h|hpp)$")

# shared source must be symlinked, passed to kbuild, and included in user-mode library
set(srcs_shared_abs ${manifest_files})
list(FILTER srcs_shared_abs INCLUDE REGEX "\\.(c|cpp)$")

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
# Kernel Lib
# =====================================================================================================================

# bare user-mode object library containing shared source
add_library(kernel_lib OBJECT
    shim/linux/compiler.h
    shim/linux/printk.h
    shim/linux/types.h

    ${hdrs_shared_abs}
    ${srcs_shared_abs}
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

# =====================================================================================================================
# Kernel Nav
# =====================================================================================================================

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

set(kbuild_shadow_root "${CMAKE_BINARY_DIR}/shadow")

# ---------------------------------------------------------------------------------------------------------------------
# Source Symlinks
# ---------------------------------------------------------------------------------------------------------------------

# collect abs files to shadow
list(APPEND srcs_abs
    ${hdrs_shared_abs}
    ${srcs_shared_abs}
)

# collect rel files to shadow
set(srcs_rel
    ${hdrs_kernel}
    ${srcs_kernel}
)
foreach(src_rel IN LISTS srcs_rel)
    # resolve absolute paths
    get_filename_component(src_abs "${src_rel}" ABSOLUTE)
    list(APPEND srcs_abs "${src_abs}")
endforeach()

# symlink shadowed files
foreach(src_abs IN LISTS srcs_abs)
    # get path relative to root src directory
    file(RELATIVE_PATH src_rel "${PROJECT_SOURCE_DIR}/src" "${src_abs}")

    # get just path relative to root
    get_filename_component(subdir "${src_rel}" DIRECTORY)

    # create symlink relative to shadow root
    set(dst_dir "${kbuild_shadow_root}/${subdir}")
    set(dst "${kbuild_shadow_root}/${src_rel}")
    message("src_abs = ${src_abs}, src_rel = ${src_rel}, subdir = ${subdir}, dst_dir = ${dst_dir}, dst = ${dst}")
    list(APPEND srcs_kbuild_rel "${src_rel}")
    list(APPEND srcs_kbuild_abs "${dst}")
    add_custom_command(
        OUTPUT "${dst}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${dst_dir}"
        COMMAND "${CMAKE_COMMAND}" -E create_symlink "${src_abs}" "${dst}"
        DEPENDS "${src_abs}"
        COMMENT "shadowing ${src_rel}"
        VERBATIM
    )

    # install to dkms
    install(FILES "${src_abs}" DESTINATION "${dkms_dst_dir}" COMPONENT dkms)
endforeach()

# symlink makefile
set(makefile "Makefile")
set(makefile_src_abs "${CMAKE_CURRENT_SOURCE_DIR}/${makefile}")
set(makefile_dst "${kbuild_shadow_root}/${makefile}")
add_custom_command(
    OUTPUT "${makefile_dst}"
    COMMAND "${CMAKE_COMMAND}" -E create_symlink "${makefile_src_abs}" "${makefile_dst}"
    DEPENDS "${makefile_src_abs}"
    COMMENT "shadowing ${makefile}"
    VERBATIM
)
install(FILES "${makefile}" DESTINATION "${dkms_dst_dir}" COMPONENT dkms)

# ---------------------------------------------------------------------------------------------------------------------
# Generated Kbuild
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
set(objs_kbuild ${srcs_kbuild_rel})
list(TRANSFORM objs_kbuild REPLACE "\\.[^.]+$" ".o")
list(JOIN objs_kbuild " " objs_kbuild)

# generate and install
set(kbuild "Kbuild")
set(kbuild_output "${project_name_underscores}.o")
set(kbuild_gen "${kbuild_shadow_root}/${kbuild}")
configure_file("${kbuild}.in" "${kbuild_gen}" @ONLY)
install(FILES "${kbuild_gen}" DESTINATION "${dkms_dst_dir}" COMPONENT dkms)

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
    DEPENDS "${srcs_kbuild_abs}" "${makefile_dst}" "${kbuild_gen}"
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
