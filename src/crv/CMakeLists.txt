# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frank Secilia
#
# main library cmake
#
# This file contains the main library. It contains just about all of the code other than the kernel, test, and ui.

add_library(lib)
target_sources(lib PRIVATE
    math/arg_min_max.hpp
    math/compensated_accumulator.hpp
    math/error_metrics.hpp
    math/fixed/float_conversions.hpp
    math/fixed/io.hpp
    math/jet/jet.hpp
    math/io.hpp
    math/io.cpp
    math/stats.hpp
)
target_sources_kernel(lib
    lib.hpp
    lib.cpp
    math/division/instruction.hpp
    math/division/long_divider.hpp
    math/division/result.hpp
    math/fixed/fixed.hpp
    math/fixed/exp2.hpp
    math/fixed/exp2_neg_m1.hpp
    math/int_traits.hpp
    math/integer.hpp
    math/limits.hpp
    math/rounding_modes.hpp
    traits.hpp
)
target_include_directories(lib PUBLIC "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>")
set_target_properties(lib
    PROPERTIES
        OUTPUT_NAME curves
        VERSION ${PROJECT_VERSION}
        SOVERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}"
)
target_link_libraries(lib project_options)

if (enable_testing)
    add_subdirectory(test)

    add_executable(unit_tests)
    target_sources(unit_tests PRIVATE
        math/arg_min_max_test.cpp
        math/compensated_accumulator_test.cpp
        math/division/instruction_test.cpp
        math/division/long_divider_test.cpp
        math/error_metrics_test.cpp
        math/fixed/test/exp2_neg_m1.cpp
        math/fixed/test/fixed.cpp
        math/fixed/test/float_conversions.cpp
        math/jet/test/jet.cpp
        math/jet/test/nested_jet.cpp
        math/io_test.cpp
        math/int_traits_test.cpp
        math/integer_test.cpp
        math/limits_test.cpp
        math/rounding_modes_test.cpp
        math/stats_test.cpp
        traits_test.cpp
    )
    target_link_libraries(unit_tests PUBLIC
        lib
        test
    )
    gtest_discover_tests(unit_tests)
endif()

add_subdirectory(kernel)
add_subdirectory(ui)
