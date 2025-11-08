# SPDX-License-Identifier: MIT
# Copyright (c) 2025 Frank Secilia
#
# This CMake script kicks off the kernel module's Kbuild when necessary and
# builds a userland library from the common files in the module.
#
# The module build is driven by the Kbuild file, `driver/Kbuild`. That file is
# the source of truth for the files in the module. CMake asks the Kbuild for
# two lists of .c files:
#
#   1. Module Sources: Files specific to the module (e.g., `entry_point.c`).
#      These are only built by the Kbuild.

#   2. Common Sources: Files shared between the module and library (e.g.,
#      `real.c`). These are built by both the Kbuild for the module and by
#      CMake in the library.
#
# These common sources are c implementations of things like fixed point math
# and the individual sensitivity curves. They are not anything that crosses the
# UAPI.
#
# CMake tracks dependencies for the library normally. When a common header
# changes, the library is rebuilt. The module's custom target depends on the
# library, so when the library is rebuilt, the module is rebuilt. This means
# when a common header changes, the module is rebuilt.
#
# This leaves a dependency gap: headers for the module are not part of the
# common library and aren't tracked automatically. These headers must be
# tracked manually. The tracked files are listed in `module_headers`, below.

# -----------------------------------------------------------------------------
# Project Files
# -----------------------------------------------------------------------------

# Define the list of kernel-only module headers.
#
# This is a manually-updated list of module-specific headers. It mirrors the
# file list in driver/Kbuild. This duplication is unavoidable.
#
# Headers for kernel-only sources are not tracked by the common library build.
# We list them here manually so the kernel module's custom build command can
# depend on them. This way, modifying any kernel header, common or module-only,
# correctly triggers a rebuild of the kernel module.
list(APPEND module_headers
  driver/entry_point.h
)

# File list for common lib unit test project.
list(APPEND common_test_files
  common_test.cpp
  common_test.hpp
  math64_test.cpp
)

# -----------------------------------------------------------------------------
# Path Configuration
# -----------------------------------------------------------------------------

# Define input and output dirs.
set(driver_bin_dir "${CMAKE_CURRENT_BINARY_DIR}/driver")
set(driver_src_dir "${CMAKE_CURRENT_SOURCE_DIR}/driver")

# -----------------------------------------------------------------------------
# Kbuild Export
# -----------------------------------------------------------------------------

# Write version.mk for the Kbuild.
#
# This is how we export the CMake build version to the kernel module.
file(WRITE
  "${driver_src_dir}/version.mk" "CURVES_VERSION := ${PROJECT_VERSION}\n"
)

# -----------------------------------------------------------------------------
# Kbuild Import
# -----------------------------------------------------------------------------

# Get common sources from Kbuild.
execute_process(
  COMMAND ${CMAKE_COMMAND} -E echo "Getting common sources..."
  COMMAND make -f kbuild_query.mk print-curves-common-sources
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  OUTPUT_VARIABLE common_sources
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
separate_arguments(common_sources UNIX_COMMAND ${common_sources})

# Get module sources from Kbuild.
execute_process(
  COMMAND ${CMAKE_COMMAND} -E echo "Getting module sources..."
  COMMAND make -f kbuild_query.mk print-curves-module-sources
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  OUTPUT_VARIABLE module_sources
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
separate_arguments(module_sources UNIX_COMMAND ${module_sources})

# -----------------------------------------------------------------------------
# Project Definition
# -----------------------------------------------------------------------------

# Add common library containing common sources.
add_library(common_library ${common_sources})
set_target_properties(common_library
  PROPERTIES
    OUTPUT_NAME curves_common
    VERSION ${PROJECT_VERSION}
    SOVERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}"
)

# Disable warnings.
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  target_compile_options(common_library PUBLIC
    # This is temporary until we get a few modules populated.
    -Wno-empty-translation-unit

    -Wno-old-style-cast
    -Wno-reserved-macro-identifier
  )
endif()

# -----------------------------------------------------------------------------
# Kbuild Delegation
# -----------------------------------------------------------------------------

# Path to final build output.
set(kernel_module_output "${driver_bin_dir}/curves.ko")

# Define base make command with no target listed.
set(kernel_make_command make -sC "/lib/modules/${CMAKE_SYSTEM_VERSION}/build"
    M=${driver_src_dir}
    MO=${driver_bin_dir}
)

# Kick off Kbuild.
add_custom_command(
  OUTPUT "${kernel_module_output}"
  COMMAND ${kernel_make_command} modules
  WORKING_DIRECTORY "${driver_bin_dir}"
  DEPENDS
    ${module_sources}
    ${module_headers}
    common_library
  COMMENT "Building kernel module..."
  VERBATIM
  USES_TERMINAL
)

# Add named target to kick off Kbuild.
add_custom_target(kernel_module ALL DEPENDS "${kernel_module_output}")

# Add named target for cleaning the Kbuild.
add_custom_target(kernel_module_clean
  COMMAND ${kernel_make_command} clean
  WORKING_DIRECTORY "${driver_bin_dir}"
  COMMENT "Cleaning kernel module..."
  VERBATIM
  USES_TERMINAL
)

# -----------------------------------------------------------------------------
# Test Project
# -----------------------------------------------------------------------------

if (enable_testing)
  add_executable(common_test ${common_test_files})
  target_include_directories(common_test PUBLIC
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>"
  )
  target_link_libraries(common_test PUBLIC
    common_library
    GTest::gmock_main
    GTest::gtest_main
    GTest::gmock
    GTest::gtest
  )

  configure_test_target_warnings(common_test)
  if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(common_test PUBLIC
      -Wno-unused-function
    )
  endif()

  gtest_discover_tests(common_test)
endif()
