# SPDX-License-Identifier: MIT
# Copyright (c) 2025 Frank Secilia
#
# This script defines the part of the ui that is framework agnostic.

# -----------------------------------------------------------------------------
# Ui Lib
# -----------------------------------------------------------------------------

list(APPEND ui_files
  rendering/spline_sampler.hpp
  ui.hpp
)

list(APPEND ui_unit_tests_files
  rendering/spline_sampler_test.cpp
)

add_library(ui ${ui_files})
set_target_properties(ui
  PROPERTIES
    OUTPUT_NAME curves_ui
    VERSION ${PROJECT_VERSION}
    SOVERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}"
)
target_link_libraries(ui PUBLIC lib)

install(TARGETS ui
  # Install .a or .lib, if configured.
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}

  # Install .so, if configured.
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}

  # Install .dll, if configured.
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if (enable_testing)
  add_executable(ui_unit_tests ${ui_unit_tests_files})
  target_link_libraries(ui_unit_tests PUBLIC
    ui
    lib
    testing_lib
  )
  configure_test_target_warnings(ui_unit_tests)
  gtest_discover_tests(ui_unit_tests)
endif()

# -----------------------------------------------------------------------------
# Project Subdirectories
# -----------------------------------------------------------------------------

add_subdirectory(qt)
