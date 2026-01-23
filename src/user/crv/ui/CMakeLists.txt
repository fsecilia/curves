# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frank Secilia
#
# This file contains the qt6-specific ui.

# Let qt know not to expect repeated libraries on build command lines.
set(QT_FORCE_CMP0156_TO_VALUE NEW)

# Find Qt.
find_package(Qt6 REQUIRED COMPONENTS Widgets)

# Enable Qt's build automation.
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Main executable.
qt_add_executable(ui main.cpp)
set_target_properties(ui PROPERTIES OUTPUT_NAME ${project_name_dashes}-config)
target_include_directories(ui PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ui PRIVATE Qt6::Widgets)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(ui PRIVATE
        -Wno-ctad-maybe-unsupported
        -Wno-unsafe-buffer-usage
        -Wno-unsafe-buffer-usage-in-libc-call
    )
endif()
