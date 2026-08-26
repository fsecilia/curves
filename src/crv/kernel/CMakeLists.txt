# SPDX-License-Identifier: GPL-2.0+ OR MIT
# Copyright (c) 2026 Frank Secilia
#
# kernel module definition
#
# This file builds the kernel module through Kbuild, maintains a symlink projection for local development, and installs
# the same explicit source manifest for DKMS.

#
# source manifest
#

set(kernel_files
    abi_validation.h
    abi.c
    cxx/cassert
    control/control.c
    control/control.h
    entry_point.c
    input/abi.c
    input/handler.c
    input/handler.h
    input/pipeline_decision.h
)

#
# kernel-specific build settings
#

set(kernel_cxx_options
    -ffreestanding
    -fno-exceptions
    -fno-rtti
    -fno-threadsafe-statics
    -fno-use-cxa-atexit
    -nostdlib
)

set(kernel_gnu_cxx_options
)

set(kernel_clang_cxx_options
    -Wno-keyword-macro
    -Wno-reserved-identifier
    -Wno-variadic-macros
)

#
# Environment
#

if(NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "curves kernel module requires a Linux host")
endif()

# if KVER is specified, target that kernel, otherwise, target the running kernel
if(DEFINED ENV{KVER} AND NOT "$ENV{KVER}" STREQUAL "")
    set(kernel_version "$ENV{KVER}")
else()
    execute_process(
        COMMAND uname -r
        RESULT_VARIABLE uname_result
        OUTPUT_VARIABLE kernel_version
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(NOT uname_result EQUAL 0 OR kernel_version STREQUAL "")
        message(FATAL_ERROR "could not determine running kernel release with uname -r")
    endif()
endif()
set(kbuild_root "/lib/modules/${kernel_version}/build")
set(kbuild_auto_conf "${kbuild_root}/include/config/auto.conf")
if(NOT EXISTS "${kbuild_auto_conf}")
    message(FATAL_ERROR "prepared kernel build '${kbuild_root}' missing include/config/auto.conf")
endif()

# find gnu make for kbuild
set(kbuild_make)
foreach(name IN ITEMS make gmake)
    unset(make_candidate)
    find_program(make_candidate NAMES "${name}" NO_CACHE)
    if(NOT make_candidate)
        continue()
    endif()

    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env LC_ALL=C "${make_candidate}" --version
        RESULT_VARIABLE make_result
        OUTPUT_VARIABLE make_output
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(make_result EQUAL 0 AND make_output MATCHES "^GNU Make([ \t]|$)")
        set(kbuild_make "${make_candidate}")
        break()
    endif()
endforeach()
if(kbuild_make STREQUAL "")
    message(FATAL_ERROR "could not find GNU Make as 'make' or 'gmake'")
endif()

# prepare kernel workspace
#
# The local workspace mirrors PROJECT_SOURCE_DIR/src. Only manifest files are symlinked into it.
# All Kbuild outputs stay in the build tree.
set(kbuild_workspace "${CMAKE_BINARY_DIR}/kernel")
set(kbuild_projection_state "${CMAKE_CURRENT_BINARY_DIR}/projection.manifest")
file(MAKE_DIRECTORY "${kbuild_workspace}")

list(TRANSFORM kernel_files PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/")
set(kernel_files ${shared_lib_kernel_files} ${kernel_files})
set(projection_source_root "${PROJECT_SOURCE_DIR}/src")
cmake_path(NORMAL_PATH projection_source_root)

# validate manifest then derive projected relative paths and Kbuild object list
set(projected_files)
set(kbuild_objects)
foreach(file IN LISTS kernel_files)
    cmake_path(ABSOLUTE_PATH file NORMALIZE OUTPUT_VARIABLE absolute)
    if(NOT EXISTS "${absolute}" AND NOT IS_SYMLINK "${absolute}")
        message(FATAL_ERROR "kernel manifest file does not exist: '${absolute}'")
    endif()

    cmake_path(IS_PREFIX projection_source_root "${absolute}" NORMALIZE within_source_root)
    if(NOT within_source_root)
        message(FATAL_ERROR "kernel manifest file lies outside '${projection_source_root}': '${absolute}'")
    endif()

    cmake_path(
        RELATIVE_PATH absolute
        BASE_DIRECTORY "${projection_source_root}"
        OUTPUT_VARIABLE relative
    )
    if(relative STREQUAL ""
        OR relative STREQUAL "."
        OR relative MATCHES "(^|/)\\.\\.(/|$)"
        OR relative MATCHES "[;\n\r]"
    )
        message(FATAL_ERROR "kernel manifest produced unsafe projected path '${relative}' from '${absolute}'")
    endif()

    list(FIND projected_files "${relative}" duplicate_index)
    if(NOT duplicate_index EQUAL -1)
        message(FATAL_ERROR "kernel manifest maps more than one entry to '${relative}'")
    endif()
    list(APPEND projected_files "${relative}")

    if(relative MATCHES "\\.(c|cc|cpp|cxx|S)$")
        string(REGEX REPLACE "\\.[^.]+$" ".o" object "${relative}")
        list(FIND kbuild_objects "${object}" object_collision_index)
        if(NOT object_collision_index EQUAL -1)
            message(FATAL_ERROR "kernel compilation units collide on Kbuild object '${object}'")
        endif()
        list(APPEND kbuild_objects "${object}")
    endif()
endforeach()

if(NOT kbuild_objects)
    message(FATAL_ERROR "kernel manifest contains no compilation units")
endif()

#
# Development Projection
#

# reconcile workspace with previous manifest
set(previous_projected_files)
set(previous_projection_text)
if(EXISTS "${kbuild_projection_state}")
    file(READ "${kbuild_projection_state}" previous_projection_text)
    string(REGEX REPLACE "\n$" "" previous_projection_list_text "${previous_projection_text}")
    if(NOT previous_projection_list_text STREQUAL "")
        string(REPLACE "\n" ";" previous_projected_files "${previous_projection_list_text}")
    endif()
endif()

set(projection_changed FALSE)
set(stale_projected_files "${previous_projected_files}")
if(projected_files)
    list(REMOVE_ITEM stale_projected_files ${projected_files})
endif()
foreach(relative IN LISTS stale_projected_files)
    if(relative STREQUAL "" OR IS_ABSOLUTE "${relative}" OR relative MATCHES "(^|/)\\.\\.(/|$)" OR relative MATCHES "[;\n\r]")
        message(FATAL_ERROR "invalid path '${relative}' in private kernel projection state")
    endif()

    set(destination "${kbuild_workspace}/${relative}")
    if(IS_SYMLINK "${destination}")
        file(REMOVE "${destination}")
        set(projection_changed TRUE)
    elseif(EXISTS "${destination}")
        message(FATAL_ERROR "stale projected path is occupied by a non-symlink: '${destination}'")
    endif()
endforeach()

foreach(source relative IN ZIP_LISTS kernel_files projected_files)
    set(destination "${kbuild_workspace}/${relative}")
    get_filename_component(destination_directory "${destination}" DIRECTORY)
    file(MAKE_DIRECTORY "${destination_directory}")

    if(IS_SYMLINK "${destination}")
        file(READ_SYMLINK "${destination}" existing_source)
        if(existing_source STREQUAL "${source}")
            continue()
        endif()
        file(REMOVE "${destination}")
        set(projection_changed TRUE)
    elseif(EXISTS "${destination}")
        message(FATAL_ERROR "projected source path is occupied by a non-symlink: '${destination}'")
    else()
        set(projection_changed TRUE)
    endif()

    file(CREATE_LINK "${source}" "${destination}" SYMBOLIC)
endforeach()

set(sorted_projected_files "${projected_files}")
list(SORT sorted_projected_files)
list(JOIN sorted_projected_files "\n" projection_text)
string(APPEND projection_text "\n")

if(NOT previous_projection_text STREQUAL projection_text)
    file(WRITE "${kbuild_projection_state}" "${projection_text}")
elseif(projection_changed)
    # file set is unchanged, but a missing or incorrect symlink was repaired
    file(TOUCH "${kbuild_projection_state}")
endif()

#
# Kbuild Generation
#

if(CMAKE_CXX_EXTENSIONS)
    set(cxx_standard_option_variable "CMAKE_CXX${CMAKE_CXX_STANDARD}_EXTENSION_COMPILE_OPTION")
else()
    set(cxx_standard_option_variable "CMAKE_CXX${CMAKE_CXX_STANDARD}_STANDARD_COMPILE_OPTION")
endif()
if(NOT DEFINED "${cxx_standard_option_variable}" OR "${${cxx_standard_option_variable}}" STREQUAL "")
    message(FATAL_ERROR "CMake did not provide ${cxx_standard_option_variable}")
endif()
set(kbuild_cxx_standard_option "${${cxx_standard_option_variable}}")

set(kbuild_shared_cxx_options "${shared_lib_kernel_cxx_options};${kernel_cxx_options}")
set(kbuild_shared_cxx_options_gnu "${shared_lib_kernel_cxx_options_gnu};${kernel_gnu_cxx_options}")
set(kbuild_shared_cxx_options_clang "${shared_lib_kernel_cxx_options_clang};${kernel_clang_cxx_options}")
list(JOIN kbuild_shared_cxx_options " " kbuild_shared_cxx_options)
list(JOIN kbuild_shared_cxx_options_gnu " " kbuild_shared_cxx_options_gnu)
list(JOIN kbuild_shared_cxx_options_clang " " kbuild_shared_cxx_options_clang)
list(JOIN kbuild_objects " " kbuild_objects_text)

# git hash for the version string
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
set(crv_version "${PROJECT_VERSION}-${git_hash}")
set(kbuild_module_name "${project_name_underscores}")
set(kbuild_control_device_name "${project_name_dashes}")

set(kbuild_file "${kbuild_workspace}/Kbuild")
set(makefile "${kbuild_workspace}/Makefile")
configure_file("Kbuild.in" "${kbuild_file}" @ONLY)
configure_file("Makefile.in" "${makefile}" @ONLY)

#
# compile_commands.json / clangd
#

# index kernel-only files so they can be refactored with clangd
add_library(kernel_index OBJECT EXCLUDE_FROM_ALL ${kernel_files})
set_property(TARGET kernel_index PROPERTY EXPORT_COMPILE_COMMANDS ON)

target_compile_definitions(kernel_index PRIVATE
    __KERNEL__
    MODULE
    "KBUILD_MODNAME=${project_name_underscores}"
    "CRV_VERSION=${crv_version}"
    "CRV_CONTROL_DEVICE_NAME=\"${kbuild_control_device_name}\""
)

execute_process(
    COMMAND uname -m
    RESULT_VARIABLE machine_result
    OUTPUT_VARIABLE kernel_machine
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT machine_result EQUAL 0)
    message(FATAL_ERROR "could not determine host architecture with uname -m for kernel_index")
endif()
if(kernel_machine MATCHES "^(x86_64|i[3-6]86)$")
    set(kernel_arch x86)
elseif(kernel_machine MATCHES "^(aarch64|arm64)$")
    set(kernel_arch arm64)
elseif(kernel_machine MATCHES "^arm")
    set(kernel_arch arm)
elseif(kernel_machine MATCHES "^(riscv64|riscv32)$")
    set(kernel_arch riscv)
else()
    message(FATAL_ERROR "kernel_index does not know the Kbuild arch directory for host machine '${kernel_machine}'")
endif()

# find kernel source tree corresponding to the prepared build tree
if(EXISTS "${kbuild_root}/source/include/linux/compiler.h")
    set(kernel_source_root "${kbuild_root}/source")
elseif(EXISTS "${kbuild_root}/include/linux/compiler.h")
    # in-tree kernel build
    set(kernel_source_root "${kbuild_root}")
elseif(EXISTS "/lib/modules/${kernel_version}/source/include/linux/compiler.h")
    set(kernel_source_root "/lib/modules/${kernel_version}/source")
else()
    message(FATAL_ERROR
        "could not locate the kernel source tree corresponding to '${kbuild_root}'"
    )
endif()

target_include_directories(kernel_index PRIVATE
    "${PROJECT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/cxx"

    "${kernel_source_root}/arch/${kernel_arch}/include"
    "${kbuild_root}/arch/${kernel_arch}/include/generated"

    "${kernel_source_root}/include"
    "${kbuild_root}/include"

    "${kernel_source_root}/arch/${kernel_arch}/include/uapi"
    "${kbuild_root}/arch/${kernel_arch}/include/generated/uapi"

    "${kernel_source_root}/include/uapi"
    "${kbuild_root}/include/generated/uapi"
)

target_compile_options(kernel_index PRIVATE
    -include "${kernel_source_root}/include/linux/compiler-version.h"
    -include "${kernel_source_root}/include/linux/kconfig.h"
    -include "${kernel_source_root}/include/linux/compiler_types.h"

    $<$<COMPILE_LANGUAGE:C>:-ffreestanding>
    "$<$<COMPILE_LANGUAGE:CXX>:${shared_lib_kernel_cxx_options}>"
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(kernel_index PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:${shared_lib_kernel_cxx_options_gnu}>")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(kernel_index PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:${shared_lib_kernel_cxx_options_clang}>")
endif()

#
# Build Targets
#

set(kbuild_module_file "${kbuild_workspace}/${kbuild_module_name}.ko")
add_custom_command(
    OUTPUT "${kbuild_module_file}"
    COMMAND "${kbuild_make}" --no-print-directory -C "${kbuild_workspace}"
        "KERNEL_DIR=${kbuild_root}"
        "CRV_DEBUG=$<IF:$<CONFIG:Debug>,1,0>"
        modules
    DEPENDS
        ${kernel_files}
        "${kbuild_projection_state}"
        "${kbuild_file}"
        "${makefile}"
        "${kbuild_auto_conf}"
    COMMENT "building kernel module ${kbuild_module_name}.ko"
    VERBATIM
    USES_TERMINAL
    JOB_SERVER_AWARE TRUE
)
add_custom_target(kbuild DEPENDS "${kbuild_module_file}")

add_custom_target(clean_kbuild
    COMMAND "${kbuild_make}" --no-print-directory -C "${kbuild_workspace}"
        "KERNEL_DIR=${kbuild_root}"
        clean
    COMMENT "cleaning kernel module ${kbuild_module_name}.ko"
    VERBATIM
    USES_TERMINAL
    JOB_SERVER_AWARE TRUE
)

#
# Installation
#

foreach(file relative IN ZIP_LISTS kernel_files projected_files)
    get_filename_component(relative_directory "${relative}" DIRECTORY)
    if(relative_directory STREQUAL "")
        set(destination "${dkms_dst_dir}")
    else()
        set(destination "${dkms_dst_dir}/${relative_directory}")
    endif()
    install(FILES "${file}" DESTINATION "${destination}" COMPONENT dkms)
endforeach()

install(FILES
    "${kbuild_file}"
    "${makefile}"
    DESTINATION "${dkms_dst_dir}"
    COMPONENT dkms
)
