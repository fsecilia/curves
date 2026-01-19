# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frank Secilia

# ---------------------------------------------------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------------------------------------------------

# base name of driver
set(driver_name ${PROJECT_NAME}_mouse_acceleration)

# driver headers
set(hdrs_driver
    fpu_guard.hpp
    math.hpp
)

# source files shared by driver and lib requiring floating point
set(srcs_driver_lib_float
    math.cpp
)

# source files shared by driver and lib not requiring floating point
set(srcs_driver_lib
    fpu_guard.cpp
)

# source files exclusive to driver
set(srcs_driver entry_point.c)

# source files exclusive to lib
set(srcs_lib
    shim/linux/compiler.h
    shim/linux/printk.h
    shim/linux/types.h
)

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

# ---------------------------------------------------------------------------------------------------------------------
# Shadow Build
# ---------------------------------------------------------------------------------------------------------------------

# configure shadow build
#
# The shadow build is an out-of-tree, modified copy of the source tree where the Kbuild can find everything and write
# its output locally. We symlink static content, like source files, and write generated files, like the Kbuild file
# itself.
set(shadow_root ${CMAKE_BINARY_DIR}/shadow)
set(staging_root ${shadow_root}/staging)
set(pkg_root ${staging_root}/pkg)

# configure dkms
set(dkms dkms.conf)
set(dkms_staged ${staging_root}/${dkms})
configure_file(${dkms}.in ${dkms_staged} @ONLY)
set(dkms_dest ${CMAKE_INSTALL_PREFIX}/usr/src/${driver_name}-${PROJECT_VERSION})

# configure udev
set(udev udev.rules)
set(udev_staged ${driver_name})
string(REPLACE "_" "-" udev_staged ${udev_staged})
set(udev_staged ${staging_root}/99-${udev_staged}.rules)
configure_file(${udev}.in ${udev_staged} @ONLY)
set(udev_dest ${CMAKE_INSTALL_PREFIX}/etc/udev/rules.d)

# symlink static content
set(srcs_shadow ${hdrs_driver} ${srcs_driver_lib} ${srcs_driver_lib_float} ${srcs_driver} Makefile)
foreach(src ${srcs_shadow})
    # src is relative; extract it and convert to abs dst
    get_filename_component(dst_dir ${src} DIRECTORY)
    get_filename_component(dst_dir ${shadow_root}/${dst_dir} ABSOLUTE)

    # get abs src
    get_filename_component(src_dir ${src} ABSOLUTE)
    get_filename_component(src_dir ${src_dir} DIRECTORY)

    add_custom_command(
        OUTPUT ${dst_dir}/${src}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${dst_dir}
        COMMAND ${CMAKE_COMMAND} -E create_symlink ${src_dir}/${src} ${dst_dir}/${src}
        DEPENDS ${src_dir}/${src}
        COMMENT "Symlinking ${src_dir}/${src} to ${dst_dir}/${src}..."
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
set(kbuild_staged ${shadow_root}/${kbuild})

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
    make -sC ${kbuild_root} V=1
    CC="${CMAKE_C_COMPILER}"
    CXX="${CMAKE_CXX_COMPILER}"
    M=${shadow_root}
)

# kick off kbuild
add_custom_command(
    OUTPUT ${shadow_root}/${kbuild_output}
    COMMAND ${kbuild_make_command} modules
    WORKING_DIRECTORY ${shadow_root}
    DEPENDS ${srcs_shadow} ${kbuild_staged}
    COMMENT "Building kernel module... ${kbuild_output}"
    VERBATIM
    USES_TERMINAL
)
add_custom_target(kbuild ALL DEPENDS ${shadow_root}/${kbuild_output})

# clean kbuild
add_custom_target(clean_kbuild
    COMMAND ${kbuild_make_command} clean
    WORKING_DIRECTORY ${shadow_root}
    COMMENT "Cleaning kernel module..."
    VERBATIM
    USES_TERMINAL
)
