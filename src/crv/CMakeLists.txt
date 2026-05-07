# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frank Secilia
#
# main library cmake
#
# This file contains the main library. It contains just about all of the code other than the kernel module itself,
# tests, and ui.

add_library(lib)
target_sources(lib PRIVATE
    math/arg_min_max.hpp
    math/compensated_accumulator.hpp
    math/elementwise_max.hpp
    math/error_metrics.hpp
    math/fixed/fixed_limits.hpp
    math/fixed/float_conversions.hpp
    math/fixed/io.hpp
    math/float_limits.hpp
    math/io.cpp
    math/io.hpp
    math/jet/jet.hpp
    math/quadrature/adaptive_integrator.hpp
    math/quadrature/antiderivative.hpp
    math/quadrature/bisector.hpp
    math/quadrature/integral.hpp
    math/quadrature/rules.hpp
    math/quadrature/segment.hpp
    math/quadrature/stack.hpp
    math/quadrature/subdivider.hpp
    math/spline/construction/approximant.hpp
    math/spline/construction/defect_checks/monotonicity.hpp
    math/spline/construction/defect_checks/overflow.hpp
    math/spline/construction/error_norm.hpp
    math/spline/construction/function_sampler.hpp
    math/spline/construction/hermite_converter.hpp
    math/spline/construction/interval.hpp
    math/spline/construction/node_generator.hpp
    math/spline/construction/quantizer.hpp
    math/spline/construction/residual_estimator.hpp
    math/spline/construction/segment_derivative.hpp
    math/spline/construction/segment_factory.hpp
    math/spline/construction/weight_function.hpp
    math/spline/construction/workspace.hpp
    math/stats.hpp
    priority_queue.hpp
)
target_sources_kernel(lib
    algorithm.hpp
    bitwise_enum.hpp
    kernel/cxx_build_test.cpp
    kernel/cxx_build_test.h
    lib.cpp
    lib.hpp
    math/abs.hpp
    math/cmp.hpp
    math/division/concepts.hpp
    math/division/divider.hpp
    math/division/hardware_divider.hpp
    math/division/io.hpp
    math/division/qr_pair.hpp
    math/division/shifted_int_divider.hpp
    math/division/wide_divider.hpp
    math/fixed/exp2_neg_m1.cpp
    math/fixed/exp2_neg_m1.hpp
    math/fixed/exp2.hpp
    math/fixed/fixed.hpp
    math/fixed/fma.hpp
    math/int_traits.hpp
    math/integer.hpp
    math/limits.hpp
    math/linear.hpp
    math/rounding_mode.hpp
    math/saturation.hpp
    math/scalar_traits.hpp
    math/shifter.hpp
    math/spline/polynomial.hpp
    math/spline/segment_locator.hpp
    math/spline/segment.hpp
    math/spline/spline.hpp
    prefetcher.hpp
    ranges.hpp
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
        algorithm_test.cpp
        bitwise_enum_test.cpp
        math/abs_test.cpp
        math/arg_min_max_test.cpp
        math/cmp_test.cpp
        math/compensated_accumulator_test.cpp
        math/division/concepts_test.cpp
        math/division/hardware_divider_test.cpp
        math/division/io_test.cpp
        math/division/shifted_int_divider_test.cpp
        math/division/wide_divider_test.cpp
        math/elementwise_max_test.cpp
        math/error_metrics_test.cpp
        math/fixed/exp2_neg_m1_test.cpp
        math/fixed/fixed_limits_test.cpp
        math/fixed/fixed_test.cpp
        math/fixed/float_conversions_test.cpp
        math/fixed/fma_test.cpp
        math/fixed/rsqrt_test.cpp
        math/float_limits_test.cpp
        math/int_traits_test.cpp
        math/integer_test.cpp
        math/io_test.cpp
        math/jet/test/jet.cpp
        math/jet/test/nested_jet.cpp
        math/limits_test.cpp
        math/linear_test.cpp
        math/quadrature/adaptive_integrator_test.cpp
        math/quadrature/antiderivative_test.cpp
        math/quadrature/bisector_test.cpp
        math/quadrature/integral_test.cpp
        math/quadrature/integration_test.cpp
        math/quadrature/rules_test.cpp
        math/quadrature/stack_test.cpp
        math/quadrature/subdivider_test.cpp
        math/rounding_mode_test.cpp
        math/scalar_traits_test.cpp
        math/shifter_test.cpp
        math/spline/construction/approximant_test.cpp
        math/spline/construction/defect_checks/monotonicity_test.cpp
        math/spline/construction/defect_checks/overflow_test.cpp
        math/spline/construction/error_norm_test.cpp
        math/spline/construction/function_sampler_test.cpp
        math/spline/construction/hermite_converter_test.cpp
        math/spline/construction/interval_test.cpp
        math/spline/construction/node_generator_test.cpp
        math/spline/construction/quantizer_test.cpp
        math/spline/construction/residual_estimator_test.cpp
        math/spline/construction/segment_derivative_test.cpp
        math/spline/construction/segment_factory_test.cpp
        math/spline/construction/weight_function_test.cpp
        math/spline/construction/workspace_test.cpp
        math/spline/polynomial_test.cpp
        math/spline/segment_locator_test.cpp
        math/spline/segment_test.cpp
        math/spline/spline_test.cpp
        math/stats_test.cpp
        prefetcher_test.cpp
        priority_queue_test.cpp
        ranges_test.cpp
        traits_test.cpp
    )
    target_link_libraries(unit_tests PUBLIC
        lib
        testing
    )
    gtest_discover_tests(unit_tests)
    add_dependencies(build_tests unit_tests)

    if (BUILD_INTEGRATION_TESTS)
        add_executable(spline_integration_test
            math/spline/construction/spline_integration_test.cpp
        )
        target_link_libraries(spline_integration_test PRIVATE lib testing)
    endif()
endif()

add_subdirectory(kernel)
add_subdirectory(ui)
