# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frank Secilia
#
# main library cmake
#
# This file contains the main library. It contains just about all of the code other than the kernel, test, and ui.

add_library(lib)
target_sources(lib PRIVATE
    math/int_cast.hpp
)
target_sources_kernel(lib
    lib.hpp
    lib.cpp
    math/fixed.hpp
)
target_include_directories(lib PUBLIC "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>")
set_target_properties(lib
    PROPERTIES
        OUTPUT_NAME curves
        VERSION ${PROJECT_VERSION}
        SOVERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}"
)

if (enable_testing)
    add_subdirectory(test)

    add_executable(unit_tests)
    target_sources(unit_tests PRIVATE
        math/fixed_test.cpp
    )
    target_link_libraries(unit_tests PUBLIC
        lib
        test
    )
    gtest_discover_tests(unit_tests)
endif()

add_subdirectory(kernel)
add_subdirectory(ui)
