# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frank Secilia
#
# main library cmake
#
# This file contains the main library. It contains just about all of the code other than the kernel module itself,
# tests, and ui.

add_library(lib)
set_target_properties(lib
    PROPERTIES
        OUTPUT_NAME curves
        VERSION ${PROJECT_VERSION}
        SOVERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}"
)

target_sources(lib PRIVATE
    bit.hpp
    concepts.hpp
    i18n.cpp
    i18n.hpp
    io/unique_fd.hpp
    math/arg_min_max.hpp
    math/compensated_accumulator.hpp
    math/complex_traits.hpp
    math/complex.hpp
    math/conjugate_quantizer.hpp
    math/elementwise_max.hpp
    math/error_metrics.hpp
    math/fixed/fixed_limits.hpp
    math/fixed/float_conversions.hpp
    math/fixed/io.hpp
    math/fixed/quantizer.hpp
    math/float_limits.hpp
    math/inverse.hpp
    math/io.cpp
    math/io.hpp
    math/jet/jet.hpp
    math/lambert.hpp
    math/polynomial.hpp
    math/stats.hpp
    model/composed_curve.hpp
    model/config.hpp
    model/curves/concepts.hpp
    model/curves/curves.hpp
    model/curves/log_normal.hpp
    model/curves/synchronous.hpp
    model/curves/traits.hpp
    model/shaping/construction/limit_engager.hpp
    model/shaping/construction/limit_factory.hpp
    model/shaping/construction/limit_locator.hpp
    model/shaping/params.hpp
    model/shaping/shaped_curve.hpp
    model/shaping/stages/input.hpp
    model/shaping/stages/output.hpp
    model/shaping/transforms/composite.hpp
    model/shaping/transforms/input_affine.hpp
    model/shaping/transforms/limit.hpp
    model/shaping/transforms/offset.hpp
    model/shaping/transforms/output_affine.hpp
    model/shaping/transitions/quadratic.hpp
    model/shaping/transitions/smootherstep_integral.hpp
    overloaded.hpp
    priority_queue.hpp
    quadrature/adaptive_integrator.hpp
    quadrature/antiderivative.hpp
    quadrature/bisector.hpp
    quadrature/integral.hpp
    quadrature/rules.hpp
    quadrature/segment.hpp
    quadrature/stack.hpp
    quadrature/subdivider.hpp
    reflection/constraints.hpp
    reflection/enum.hpp
    reflection/param.hpp
    sequential_enum_name_map.hpp
    serialization/deserializer.hpp
    serialization/exceptions.cpp
    serialization/exceptions.hpp
    serialization/reader.hpp
    serialization/serializer.hpp
    serialization/toml/archive.cpp
    serialization/toml/archive.hpp
    serialization/toml/concepts.hpp
    serialization/toml/error_reporting.cpp
    serialization/toml/error_reporting.hpp
    serialization/toml/reader_adapter.hpp
    serialization/toml/toml.hpp
    serialization/toml/writer_adapter.hpp
    serialization/writer.hpp
    spline/construction/segment/amr/approximant.hpp
    spline/construction/segment/amr/bisection.hpp
    spline/construction/segment/amr/error_metric.hpp
    spline/construction/segment/amr/function_sampler.hpp
    spline/construction/segment/amr/interval.hpp
    spline/construction/segment/amr/node_generator.hpp
    spline/construction/segment/amr/residual_estimator.hpp
    spline/construction/segment/amr/subdivision_predicate.hpp
    spline/construction/segment/amr/subdivision.hpp
    spline/construction/segment/field_packer.hpp
    spline/construction/segment/segment_factory.hpp
    spline/construction/segment/segment_packer.hpp
    spline/construction/segment/segment_quantizer.hpp
    spline/construction/segment/shift_planner.hpp
    spline/construction/spline/amr/assembler.hpp
    spline/construction/spline/amr/refinement_pool_seeder.hpp
    spline/construction/spline/amr/refiner.hpp
    spline/construction/spline/amr/seed/critical_point_conditioner.hpp
    spline/construction/spline/amr/seed/dyadic_stride_calculator.hpp
    spline/construction/spline/amr/seed/span_decomposer.hpp
    spline/construction/spline/amr/seed/subdomain_factory.hpp
    spline/construction/spline/amr/spline_generator.hpp
    spline/construction/spline/amr/typestates.hpp
    spline/construction/spline/amr/workspace.hpp
    spline/construction/spline/tangent_extender.hpp
    spline/construction/weight_functions/exponential_decay.hpp
    spline/construction/weight_functions/hyperbolic_decay.hpp
    spline/construction/weight_functions/uniform.hpp
    spline/spline_factory_policy.hpp
    spline/spline_factory.hpp
    tuple.hpp
    variant.hpp
)

include(config.cmake)
configure_shared(lib PUBLIC)

target_compile_options(lib PUBLIC
    -Wdouble-promotion
    -Wfloat-conversion
)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(lib PUBLIC
        -Wno-exit-time-destructors
        -Wno-float-equal
        -Wno-global-constructors
    )
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(lib PUBLIC
        -fsized-deallocation

        $<$<NOT:$<BOOL:$<TARGET_PROPERTY:DISABLE_STRICT_ALIASING>>>:-fstrict-aliasing>
        $<$<NOT:$<BOOL:$<TARGET_PROPERTY:DISABLE_STRICT_ALIASING>>>:-Wstrict-aliasing=2>
    )
    target_link_options(lib PUBLIC
        "LINKER:-z,relro"
        "LINKER:-z,now"
    )
endif()

target_include_directories(lib PUBLIC "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>")
target_link_libraries(lib tomlplusplus::tomlplusplus)

# git hash for version string
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    OUTPUT_VARIABLE git_hash
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT git_hash)
    set(git_hash standalone)
endif()
set(crv_version ${PROJECT_VERSION}-${git_hash})

option(ENABLE_ASAN "Enable AddressSanitizer (ASan) for debug builds." OFF)
if (ENABLE_ASAN)
    target_compile_options(lib PUBLIC -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(lib PUBLIC link_options_user_mode -fsanitize=address)
endif()

kontra_add_kernel_project(
    kernel_modules

    PROJECTION_ROOT "${PROJECT_SOURCE_DIR}/src"

    CMAKE_ARGUMENTS
        "-DCMAKE_CXX_STANDARD=${CMAKE_CXX_STANDARD}"
        "-DCMAKE_CXX_STANDARD_REQUIRED=${CMAKE_CXX_STANDARD_REQUIRED}"
        "-DCMAKE_CXX_EXTENSIONS=${CMAKE_CXX_EXTENSIONS}"

        "-DCRV_VERSION=${crv_version}"
)

kontra_add_kernel_module(
    driver
    PROJECT kernel_modules
    OUTPUT_NAME "${project_name_underscores}"
)

kontra_install_kernel_module(
    kernel_modules.driver
    DESTINATION "${dkms_dst_dir}"
    COMPONENT dkms
)

if (enable_testing)
    add_subdirectory(test)

    add_executable(unit_tests)
    target_sources(unit_tests PRIVATE
        algorithm_test.cpp
        bit_test.cpp
        bitwise_enum_test.cpp
        concepts_test.cpp
        i18n_test.cpp
        io/unique_fd_integration_test.cpp
        io/unique_fd_test.cpp
        math/abs_test.cpp
        math/arg_min_max_test.cpp
        math/cmp_test.cpp
        math/compensated_accumulator_test.cpp
        math/complex_test.cpp
        math/complex_traits_test.cpp
        math/conjugate_quantizer_test.cpp
        math/division/concepts_test.cpp
        math/division/divider_test.cpp
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
        math/fixed/quantizer_test.cpp
        math/fixed/rsqrt_test.cpp
        math/fixed/uabs_test.cpp
        math/float_limits_test.cpp
        math/int_traits_test.cpp
        math/integer_test.cpp
        math/inverse_test.cpp
        math/io_test.cpp
        math/jet/test/jet.cpp
        math/jet/test/nested_jet.cpp
        math/lambert_test.cpp
        math/limits_test.cpp
        math/linear_test.cpp
        math/polynomial_test.cpp
        math/rounding_mode_test.cpp
        math/saturate_cast_test.cpp
        math/scalar_traits_test.cpp
        math/shifter_test.cpp
        math/stats_test.cpp
        model/composed_curve_test.cpp
        model/config_test.cpp
        model/curves/concepts_test.cpp
        model/curves/log_normal_test.cpp
        model/curves/synchronous_test.cpp
        model/curves/test.hpp
        model/curves/traits_test.cpp
        model/shaping/construction/limit_engager_test.cpp
        model/shaping/construction/limit_factory_test.cpp
        model/shaping/construction/limit_locator_test.cpp
        model/shaping/params_test.cpp
        model/shaping/shaped_curve_test.cpp
        model/shaping/stages/input_test.cpp
        model/shaping/stages/output_test.cpp
        model/shaping/transforms/composite_test.cpp
        model/shaping/transforms/input_affine_test.cpp
        model/shaping/transforms/limit_test.cpp
        model/shaping/transforms/offset_test.cpp
        model/shaping/transforms/output_affine_test.cpp
        model/shaping/transitions/quadratic_test.cpp
        model/shaping/transitions/smootherstep_integral_test.cpp
        overloaded_test.cpp
        prefetcher_test.cpp
        priority_queue_test.cpp
        quadrature/adaptive_integrator_test.cpp
        quadrature/antiderivative_test.cpp
        quadrature/bisector_test.cpp
        quadrature/integral_test.cpp
        quadrature/integration_test.cpp
        quadrature/rules_test.cpp
        quadrature/stack_test.cpp
        quadrature/subdivider_test.cpp
        ranges_test.cpp
        reflection/constraints_test.cpp
        reflection/enum_test.cpp
        reflection/param_test.cpp
        sequential_enum_name_map_test.cpp
        serialization/deserializer_test.cpp
        serialization/reader_test.cpp
        serialization/serializer_test.cpp
        serialization/toml/archive_test.cpp
        serialization/toml/concepts_test.cpp
        serialization/toml/error_reporting_test.cpp
        serialization/toml/reader_adapter_test.cpp
        serialization/toml/writer_adapter_test.cpp
        serialization/writer_test.cpp
        spline/construction/segment/amr/approximant_test.cpp
        spline/construction/segment/amr/bisection_test.cpp
        spline/construction/segment/amr/error_metric_test.cpp
        spline/construction/segment/amr/function_sampler_test.cpp
        spline/construction/segment/amr/interval_test.cpp
        spline/construction/segment/amr/node_generator_test.cpp
        spline/construction/segment/amr/residual_estimator_test.cpp
        spline/construction/segment/amr/subdivision_predicate_test.cpp
        spline/construction/segment/amr/subdivision_test.cpp
        spline/construction/segment/field_packer_test.cpp
        spline/construction/segment/segment_factory_test.cpp
        spline/construction/segment/segment_packer_test.cpp
        spline/construction/segment/segment_quantizer_test.cpp
        spline/construction/segment/shift_planner_test.cpp
        spline/construction/spline/amr/assembler_test.cpp
        spline/construction/spline/amr/refinement_pool_seeder_test.cpp
        spline/construction/spline/amr/refiner_test.cpp
        spline/construction/spline/amr/seed/critical_point_conditioner_test.cpp
        spline/construction/spline/amr/seed/dyadic_stride_calculator_test.cpp
        spline/construction/spline/amr/seed/span_decomposer_test.cpp
        spline/construction/spline/amr/seed/subdomain_factory_test.cpp
        spline/construction/spline/amr/spline_generator_test.cpp
        spline/construction/spline/amr/workspace_test.cpp
        spline/construction/spline/tangent_extender_test.cpp
        spline/construction/weight_functions/exponential_decay_test.cpp
        spline/construction/weight_functions/hyperbolic_decay_test.cpp
        spline/construction/weight_functions/uniform_test.cpp
        spline/pipeline_config_test.cpp
        spline/segment_locator_test.cpp
        spline/segment_test.cpp
        spline/spline_factory_test.cpp
        spline/spline_test.cpp
        spline/tangent_extension_test.cpp
        traits_test.cpp
        tuple_test.cpp
        variant_test.cpp
    )
    target_link_libraries(unit_tests PUBLIC
        lib
        testing
    )
    gtest_discover_tests(unit_tests)
    add_dependencies(build_tests unit_tests)

    if (BUILD_INTEGRATION_TESTS)
        add_executable(spline_integration_test
            spline/segment_integration_test.cpp
            spline/spline_integration_test.cpp
        )
        target_link_libraries(spline_integration_test PRIVATE lib testing)
    endif()
endif()

add_subdirectory(ui)
