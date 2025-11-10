# SPDX-License-Identifier: MIT
# Copyright (c) 2025 Frank Secilia

cmake_minimum_required(VERSION 3.31)
project(curves VERSION 0.1.0)

# -----------------------------------------------------------------------------
# Dependencies
# -----------------------------------------------------------------------------

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
find_package(GTest)
if (GTEST_FOUND)
  set(enable_testing True)
endif()

# -----------------------------------------------------------------------------
# Warnings
# -----------------------------------------------------------------------------

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(
    -pedantic
    -Wall
    -Werror
    -Wextra
    -Wstrict-aliasing=2
    -Wswitch-enum
  )
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  add_compile_options(
    -Weverything

    -Wno-c++20-compat
    -Wno-c++98-compat
    -Wno-c++98-compat-pedantic
    -Wno-documentation
    -Wno-documentation-unknown-command
    -Wno-exit-time-destructors
    -Wno-global-constructors
  )
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  add_compile_options(-D_SCL_SECURE_NO_WARNINGS -D_CRT_SECURE_NO_WARNINGS
                      /WX /W4 /wd4201 /wd4250 /wd4251 /wd4275 /wd4458 /wd4459
                      /wd4576)
  add_link_options(/WX)
endif()

# -----------------------------------------------------------------------------
# Compiler
# -----------------------------------------------------------------------------

set(CMAKE_CXX_EXTENSIONS FALSE)
set(CMAKE_CXX_SCAN_FOR_MODULES FALSE)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)
set(CMAKE_VISIBILITY_INLINES_HIDDEN TRUE)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(
    -fstrict-aliasing
    $<$<COMPILE_LANGUAGE:CXX>:-fsized-deallocation>
  )
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  add_compile_options(-fsafe-buffer-usage-suggestions)
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  add_compile_options(
    $<$<COMPILE_LANGUAGE:CXX>:-fconcepts-diagnostics-depth=10>
  )
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  add_compile_options(/utf-8 "$<$<CONFIG:RELEASE>:/GS;/Gy;/Zc:inline>")
  add_link_options("$<$<CONFIG:RELEASE>:/LTCG;/OPT:ICF;/OPT:REF>")
endif()

# -----------------------------------------------------------------------------
# Installation
# -----------------------------------------------------------------------------

# Determine install directories.
include(GNUInstallDirs)

# Suppress "Up-to-date" message cmake prints for every installed file.
set(CMAKE_INSTALL_MESSAGE NEVER)


# -----------------------------------------------------------------------------
# Project Functions
# -----------------------------------------------------------------------------

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
      -Wno-unused-local-typedefs
    )
  endif()
endfunction()

# -----------------------------------------------------------------------------
# Project Subdirectories
# -----------------------------------------------------------------------------

add_subdirectory(src/curves)
