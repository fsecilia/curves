# SPDX-License-Identifier: MIT
# Copyright (c) 2025 Frank Secilia

set(library_files
  lib.cpp
  lib.hpp
)

# -----------------------------------------------------------------------------
# Generated Configuration
# -----------------------------------------------------------------------------

set(config_template_file config.hpp.in)
set(config_gen_filename config.gen.hpp)
set(config_gen_root "${CMAKE_BINARY_DIR}/gen")
set(config_gen_file "${config_gen_root}/curves/${config_gen_filename}")
configure_file("${config_template_file}" "${config_gen_file}")

add_library(lib
  ${library_files}
  "${config_template_file}"
  "${config_gen_file}"
)
target_include_directories(lib INTERFACE
  "$<BUILD_INTERFACE:${config_gen_root}>"
  "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>"
)
target_link_libraries(lib PRIVATE userland_library)
set_target_properties(lib
  PROPERTIES
    OUTPUT_NAME curves
    VERSION ${PROJECT_VERSION}
    SOVERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}"
)
