# SPDX-License-Identifier: MIT
# Copyright (c) 2025 Frank Secilia
#
# This script defines the project library and kicks off the driver's Kbuild
# when necessary.
#
# The Kbuild is driven by `driver/Kbuild`.

# -----------------------------------------------------------------------------
# Project Files
# -----------------------------------------------------------------------------

# File list for common lib unit test project.
list(APPEND common_test_files
  common_test.cpp
  common_test.hpp
  math64_test.cpp
)

# The Kbuild is the source of truth for the files in the driver. CMake asks the
# Kbuild for two lists of .c files:
#
#   1. Driver Sources: Files specific to the driver (e.g., `entry_point.c`).
#      These are only built by the Kbuild.
#   2. Common Sources: Files shared between the driver and library (e.g.,
#      `math64.c`). These are built by both the Kbuild for the driver and by
#      CMake in the library.
#
# These common sources are c implementations of things like fixed point math
# and the individual sensitivity curves. They are not anything that crosses the
# UAPI.
#
# The CMake custom command that kicks off the Kbuild depends on both driver
# sources and common sources. If any of these are modified, the Kbuild is run
# automatically. However, this leaves a dependency gap: the Kbuild only reports
# sources; it does not report headers.
#
# The list of headers must be tracked manually. This is that list.
list(APPEND driver_headers
  driver/entry_point.h
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
# This is how we export the CMake build version to the driver.
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

# Get driver sources from Kbuild.
execute_process(
  COMMAND ${CMAKE_COMMAND} -E echo "Getting driver sources..."
  COMMAND make -f kbuild_query.mk print-curves-driver-sources
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  OUTPUT_VARIABLE driver_sources
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
separate_arguments(driver_sources UNIX_COMMAND ${driver_sources})

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
set(driver_output "${driver_bin_dir}/curves.ko")

# Define base make command with no target listed.
set(driver_make_command make -sC "/lib/modules/${CMAKE_SYSTEM_VERSION}/build"
    M=${driver_src_dir}
    MO=${driver_bin_dir}
)

# Kick off Kbuild.
add_custom_command(
  OUTPUT "${driver_output}"
  COMMAND ${driver_make_command} modules
  WORKING_DIRECTORY "${driver_bin_dir}"
  DEPENDS
    ${common_sources}
    ${driver_sources}
    ${driver_headers}
  COMMENT "Building driver..."
  VERBATIM
  USES_TERMINAL
)

# Add named target to kick off Kbuild.
add_custom_target(driver ALL DEPENDS "${driver_output}")

# Add named target for cleaning the Kbuild.
add_custom_target(clean_driver
  COMMAND ${driver_make_command} clean
  WORKING_DIRECTORY "${driver_bin_dir}"
  COMMENT "Cleaning driver..."
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
