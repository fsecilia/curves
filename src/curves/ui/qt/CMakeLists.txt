# SPDX-License-Identifier: MIT
# Copyright (c) 2025 Frank Secilia
#
# This script defines qt-specific part of the ui.

# Let qt know not to expect repeated librarys on build command lines.
set(QT_FORCE_CMP0156_TO_VALUE NEW)

# Find Qt.
find_package(Qt6 REQUIRED COMPONENTS Widgets)

# Enable Qt's build automation.
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Main executable.
qt_add_executable(curves_config
    main.cpp

    main_window/main_window.hpp
    main_window/main_window.cpp
    main_window/main_window.ui
    main_window/curve_parameter/curve_parameter.hpp
    main_window/curve_parameter/curve_parameter.cpp
    main_window/curve_parameter/curve_parameter.ui
)

target_include_directories(curves_config PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(curves_config PRIVATE
    lib
    Qt6::Widgets
)

set_target_properties(lib
  PROPERTIES
    OUTPUT_NAME curves
)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  target_compile_options(curves_config PUBLIC
    -Wno-ctad-maybe-unsupported
    -Wno-unsafe-buffer-usage
    -Wno-unsafe-buffer-usage-in-libc-call
  )
endif()
