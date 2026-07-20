# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frank Secilia

add_executable(input_capture
    input_capture.cpp
)
target_include_directories(input_capture PUBLIC "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>")
target_link_libraries(input_capture PRIVATE lib)
