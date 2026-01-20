# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frank Secilia

cmake_minimum_required(VERSION 3.31)
project(curves VERSION 0.1.0)

# Don't repeat libraries on build command lines.
cmake_policy(SET CMP0156 NEW)

# ---------------------------------------------------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------------------------------------------------

# Use ccache, if available.
find_program(ccache ccache)
if (ccache)
    set(CMAKE_C_COMPILER_LAUNCHER "${ccache}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${ccache}")
    set(CMAKE_C_LINKER_LAUNCHER "${ccache}")
    set(CMAKE_CXX_LINKER_LAUNCHER "${ccache}")
endif()

# Find system threading library.
set(THREADS_PREFER_PTHREAD_FLAG True)
find_package(Threads REQUIRED)

# Find gtest and enable testing if found.
if(NOT TARGET GTest::gtest)
    # Try finding system package normally.
    find_package(GTest QUIET)
endif()
if(NOT TARGET GTest::gtest)
    # Look where debian puts the source build.
    if (EXISTS /usr/src/googletest)
        add_subdirectory(/usr/src/googletest ${CMAKE_BINARY_DIR}/external/googletest)
    endif()
endif()
if(TARGET GTest::gtest)
    set(enable_testing True)
endif()

option(USE_SYSTEM_DEPS "Default to using system dependencies for all libraries." OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(AddTomlPlusPlus)
include(AddDink)

# ---------------------------------------------------------------------------------------------------------------------
# Warnings
# ---------------------------------------------------------------------------------------------------------------------

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    list(APPEND c_flags
        -Wall
        -Werror
        -Wextra
        -Wstrict-aliasing=2
        -Wswitch-enum
    )
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    list(APPEND c_flags
        -Weverything

        -Wno-c++23-compat
        -Wno-c++20-compat
        -Wno-c++98-compat
        -Wno-c++98-compat-pedantic
        -Wno-c99-extensions
        -Wno-documentation
        -Wno-documentation-unknown-command
        -Wno-exit-time-destructors
        -Wno-global-constructors
        -Wno-padded
        -Wno-switch-default
        -Wno-unsafe-buffer-usage
    )
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    list(APPEND c_flags
        -D_SCL_SECURE_NO_WARNINGS -D_CRT_SECURE_NO_WARNINGS

        /WX /W4 /wd4201 /wd4250 /wd4251 /wd4275 /wd4458 /wd4459 /wd4576)
    add_link_options(/WX)
endif()

# ---------------------------------------------------------------------------------------------------------------------
# Compiler
# ---------------------------------------------------------------------------------------------------------------------

set(CMAKE_CXX_EXTENSIONS FALSE)
set(CMAKE_CXX_SCAN_FOR_MODULES FALSE)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)
set(CMAKE_VISIBILITY_INLINES_HIDDEN TRUE)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    list(APPEND c_flags -fstrict-aliasing)
    list(APPEND cxx_flags -fsized-deallocation)
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    list(APPEND c_flags -fsafe-buffer-usage-suggestions)
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    list(APPEND c_flags /utf-8 "$<$<CONFIG:RELEASE>:/GS;/Gy;/Zc:inline>")
    add_link_options("$<$<CONFIG:RELEASE>:/LTCG;/OPT:ICF;/OPT:REF>")
endif()

add_compile_options(${c_flags} $<$<COMPILE_LANGUAGE:CXX>:${cxx_flags}>)

# ---------------------------------------------------------------------------------------------------------------------
# Debugging
# ---------------------------------------------------------------------------------------------------------------------

option(ENABLE_ASAN "Enable AddressSanitizer (ASan) for debug builds." OFF)
if (ENABLE_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()

# ---------------------------------------------------------------------------------------------------------------------
# Installation
# ---------------------------------------------------------------------------------------------------------------------

# Determine install directories.
include(GNUInstallDirs)

# Suppress "Up-to-date" message cmake prints for every installed file.
set(CMAKE_INSTALL_MESSAGE NEVER)


# ---------------------------------------------------------------------------------------------------------------------
# Project Functions
# ---------------------------------------------------------------------------------------------------------------------

# Disables a few warnings generated when using gtest.
function(configure_test_target_warnings target)
    if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        target_compile_options(${target} PUBLIC
            -Wno-ctad-maybe-unsupported
            -Wno-global-constructors
            -Wno-padded
            -Wno-shadow
            -Wno-shadow-field-in-constructor
            -Wno-unneeded-member-function
            -Wno-unused-member-function
            -Wno-unused-template
        )
    endif()

    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(${target} PUBLIC
            -Wno-changes-meaning
            -Wno-missing-field-initializers
            -Wno-psabi
            -Wno-unused-local-typedefs
        )
    endif()
endfunction()

# ---------------------------------------------------------------------------------------------------------------------
# Project Subdirectories
# ---------------------------------------------------------------------------------------------------------------------

add_subdirectory(src/crv)

set(enable_testing False)
add_subdirectory(preprod EXCLUDE_FROM_ALL)
