# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frank Secilia
#
# main library cmake
#
# This file contains the main library. It contains just about all of the code other than the kernel, test, and ui.

add_library(lib)
target_sources(lib PRIVATE
    lib.hpp
    lib.cpp
    math/int_cast.hpp
)
target_link_libraries(lib user_mode_kernel_shim)
target_include_directories(lib PUBLIC "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>")
set_target_properties(lib
    PROPERTIES
        OUTPUT_NAME curves
        VERSION ${PROJECT_VERSION}
        SOVERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}"
)

if (enable_testing)
    add_subdirectory(test)
endif()

add_subdirectory(kernel)
add_subdirectory(ui)
