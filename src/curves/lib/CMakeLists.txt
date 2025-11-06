# SPDX-License-Identifier: MIT
# Copyright (c) 2025 Frank Secilia

set(library_files
  lib.cpp
  lib.hpp
)

add_library(lib ${library_files})
target_link_libraries(lib PRIVATE userland_library)
set_target_properties(lib
  PROPERTIES
    OUTPUT_NAME curves
)
