# SPDX-License-Identifier: GPL-2.0+ OR MIT
# Copyright (c) 2026 Frank Secilia
#
# This module manages staging the driver for packaging or dkms, building and loading the driver for local testing, a
# usermode object library containing files shared between usermode and the driver, and unit tests for the library. The
# driver is built using the native Kbuild. Kbuild has bad tree hygine, so it is built using a staging directory in the
# build tree rather than directly in the source.

# ---------------------------------------------------------------------------------------------------------------------
# File Lists
# ---------------------------------------------------------------------------------------------------------------------
# File lists are separated into a few categories:
#  - Headers can't go in Kbuild source lists.
#  - Some files are exclusive to the driver, some to the library, and some are shared.
#  - Source files requiring the fpu require special build flags.
# Instead of filtering for these contextually, they're just kept separate and combined when they can be.
# ---------------------------------------------------------------------------------------------------------------------

# driver headers
set(hdrs_driver
    fpu_guard.hpp
    math.hpp
)

# shared source files that require floating point
set(srcs_driver_lib_float
    math.cpp
)

# shared source files that do not require floating point
set(srcs_driver_lib
)

# source files exclusive to driver
set(srcs_driver
    entry_point.c
    fpu_guard.cpp
)

# source files exclusive to lib
set(srcs_lib
    shim/linux/compiler.h
    shim/linux/printk.h
    shim/linux/types.h
)

# ---------------------------------------------------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------------------------------------------------

# base name of driver
set(driver_name ${PROJECT_NAME}_mouse_acceleration)

# driver build flags
list(APPEND driver_c_flags ${c_flags} -ffreestanding)
list(APPEND driver_cxx_flags ${cxx_flags}
    -fno-builtin
    -fno-exceptions
    -fno-rtti
    -fno-threadsafe-statics
    -fno-use-cxa-atexit
    -nostdinc++
    -nostdlib
)

# ---------------------------------------------------------------------------------------------------------------------
# Projects
# ---------------------------------------------------------------------------------------------------------------------

# create bare object library containing shared files
add_library(driver_lib OBJECT ${srcs_driver_lib} ${srcs_driver_lib_float} ${srcs_lib} ${hdrs_driver})
target_include_directories(driver_lib PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/shim>
)
target_compile_options(driver_lib PRIVATE ${driver_c_flags} $<$<COMPILE_LANGUAGE:CXX>:${driver_cxx_flags}>)

# add nav-only project to get driver into the compilation database.
add_library(driver OBJECT EXCLUDE_FROM_ALL OBJECT ${srcs_driver})
target_link_libraries(driver PRIVATE driver_lib)
add_dependencies(driver kbuild)
target_compile_definitions(driver PRIVATE
    __KERNEL__
    MODULE
    KBUILD_MODNAME=${driver_name}
)
target_include_directories(driver PRIVATE
    ${kbuild_root}/include
    ${kbuild_root}/arch/x86/include
    ${kbuild_root}/arch/x86/include/generated
    ${kbuild_root}/include/uapi
    ${kbuild_root}/arch/x86/include/uapi
    ${kbuild_root}/include/generated/uapi
)
target_compile_options(driver PRIVATE -include ${kbuild_root}/include/linux/kconfig.h)

# ---------------------------------------------------------------------------------------------------------------------
# Environment
# ---------------------------------------------------------------------------------------------------------------------

# get git hash
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

# get kernel version
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

# handle clang builds
set(kbuild_flags "")
if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    list(APPEND kbuild_flags "LLVM=1")
endif()

# =====================================================================================================================
# Build Staging
# =====================================================================================================================
# The driver build is staged for installation. It contains the driver source, the generated Kbuild and its makefile,
# generated dkms.conf, and generates udev.rules. From here, it can be built from cmake to smoke test that it compiles,
# installed directly for testing in a vm, or installed to a destdir to build packages.
# =====================================================================================================================

# specify locations
set(staging_root ${CMAKE_BINARY_DIR}/staging)

# ---------------------------------------------------------------------------------------------------------------------
# Generated Files
# ---------------------------------------------------------------------------------------------------------------------

# configure dkms
set(dkms dkms.conf)
set(dkms_staged ${staging_root}/${dkms})
configure_file(${dkms}.in ${dkms_staged} @ONLY)

# configure udev
set(udev udev.rules)
set(udev_staged ${driver_name})
string(REPLACE "_" "-" udev_staged ${udev_staged})
set(udev_staged ${staging_root}/99-${udev_staged}.rules)
configure_file(${udev}.in ${udev_staged} @ONLY)

# ---------------------------------------------------------------------------------------------------------------------
# Static Content
# ---------------------------------------------------------------------------------------------------------------------

set(kernel_dst src/kernel)

# symlink source files and the makefile
set(srcs_staging ${hdrs_driver} ${srcs_driver_lib} ${srcs_driver_lib_float} ${srcs_driver} Makefile)
foreach(src_rel ${srcs_staging})
    # resolve dst
    get_filename_component(dst_rel ${src_rel} DIRECTORY)
    get_filename_component(dst_dir ${staging_root}/${dst_rel} ABSOLUTE)
    set(dst ${dst_dir}/${src_rel})

    # resolve src
    get_filename_component(src_abs ${src_rel} ABSOLUTE)
    get_filename_component(src_dir ${src_abs} DIRECTORY)
    set(src ${src_dir}/${src_rel})

    # symlink file
    add_custom_command(
        OUTPUT ${dst}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${dst_dir}
        COMMAND ${CMAKE_COMMAND} -E create_symlink ${src} ${dst}
        DEPENDS ${src}
        COMMENT "Symlinking ${src} to ${dst}..."
        VERBATIM
    )
endforeach()

# ---------------------------------------------------------------------------------------------------------------------
# Kbuild
# ---------------------------------------------------------------------------------------------------------------------

# extract build settings from cmake library to send down to kbuild
get_target_property(driver_lib_std_version driver_lib CXX_STANDARD)
set(driver_lib_std_version_flag ${CMAKE_CXX${driver_lib_std_version}_STANDARD_COMPILE_OPTION})
set(kbuild_c_flags ${driver_c_flags})
set(kbuild_cxx_flags ${driver_lib_std_version_flag} ${driver_cxx_flags})
list(JOIN kbuild_c_flags " " kbuild_c_flags)
list(JOIN kbuild_cxx_flags " " kbuild_cxx_flags)

# configure Kbuild
set(kbuild Kbuild)
set(kbuild_output ${driver_name}.o)
set(kbuild_staged ${staging_root}/${kbuild})

# collect source files that don't need float
set(srcs_kbuild ${srcs_driver_lib} ${srcs_driver})
list(TRANSFORM srcs_kbuild REPLACE "\\.[^.]+$" ".o")
list(JOIN srcs_kbuild " " srcs_kbuild)

# collect source files that do need float
set(srcs_kbuild_float ${srcs_driver_lib_float})
list(TRANSFORM srcs_kbuild_float REPLACE "\\.[^.]+$" ".o")
list(JOIN srcs_kbuild_float " " srcs_kbuild_float)

# generate kbuild file
configure_file(${kbuild}.in ${kbuild_staged} @ONLY)

# define kbuild commandline
set(kbuild_make_command
    make -sC ${kbuild_root} ${kbuild_flags} V=1
    CC="${CMAKE_C_COMPILER}"
    CXX="${CMAKE_CXX_COMPILER}"
    M=${staging_root}
)

# kick off kbuild
add_custom_command(
    OUTPUT ${staging_root}/${kbuild_output}
    COMMAND ${kbuild_make_command} modules
    WORKING_DIRECTORY ${staging_root}
    DEPENDS ${srcs_staging} ${kbuild_staged}
    COMMENT "Building kernel module... ${kbuild_output}"
    VERBATIM
    USES_TERMINAL
)
add_custom_target(kbuild ALL DEPENDS ${staging_root}/${kbuild_output})

# clean kbuild
add_custom_target(clean_kbuild
    COMMAND ${kbuild_make_command} clean
    WORKING_DIRECTORY ${staging_root}
    COMMENT "Cleaning kernel module..."
    VERBATIM
    USES_TERMINAL
)
