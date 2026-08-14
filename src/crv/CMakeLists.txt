# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frank Secilia
#
# main library definition
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

# files specific to the library
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
    spline/construction/curve_target.hpp
    spline/construction/segment/amr/approximant.hpp
    spline/construction/segment/amr/bisection.hpp
    spline/construction/segment/amr/error_metric.hpp
    spline/construction/segment/amr/interval.hpp
    spline/construction/segment/amr/node_generator.hpp
    spline/construction/segment/amr/residual_estimator.hpp
    spline/construction/segment/amr/subdivision_predicate.hpp
    spline/construction/segment/amr/subdivision.hpp
    spline/construction/segment/amr/transfer_sample.hpp
    spline/construction/segment/field_packer.hpp
    spline/construction/segment/local_coordinate.hpp
    spline/construction/segment/segment_factory.hpp
    spline/construction/segment/segment_packer.hpp
    spline/construction/segment/segment_quantizer.hpp
    spline/construction/segment/shift_planner.hpp
    spline/construction/spline/amr/assembler.hpp
    spline/construction/spline/amr/refinement_pool_seeder.hpp
    spline/construction/spline/amr/refiner.hpp
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

# files compiled in both user mode and the kernel
set(crv_kernel_shared_files
    algorithm.hpp
    bitwise_enum.hpp
    kernel/cxx_build_test.cpp
    kernel/cxx_build_test.h
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
    math/fixed/uabs.hpp
    math/int_traits.hpp
    math/integer.hpp
    math/inverse.hpp
    math/limits.hpp
    math/linear.hpp
    math/rounding_mode.hpp
    math/saturate_cast.hpp
    math/scalar_traits.hpp
    math/shifter.hpp
    pipeline/filters/one_euro.hpp
    pipeline/filters/one_euro/cutoff_interval.hpp
    pipeline/filters/one_euro/cutoff_rate.hpp
    prefetcher.hpp
    ranges.hpp
    spline/pipeline_config.hpp
    spline/segment_locator.hpp
    spline/segment.hpp
    spline/spline.hpp
    spline/tangent_extension.hpp
    traits.hpp
)
target_sources(lib PRIVATE ${crv_kernel_shared_files})
list(TRANSFORM crv_kernel_shared_files PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/")

# c++ options shared by lib and kernel
set(crv_shared_cxx_options
    -Wall
    -Werror
    -Wextra
    -Wno-psabi
    -Wswitch
)
set(crv_gnu_shared_cxx_options
    -fdiagnostics-color=always
    -fext-numeric-literals
    -ftemplate-backtrace-limit=1
    -Wno-changes-meaning
)
set(crv_clang_shared_cxx_options
    -fcolor-diagnostics
    -fsafe-buffer-usage-suggestions
    -Weverything
    -Wno-c++20-compat
    -Wno-c++23-compat
    -Wno-c++98-compat
    -Wno-c++98-compat-pedantic
    -Wno-c++2c-compat
    -Wno-c++2c-extensions
    -Wno-c99-extensions
    -Wno-ctad-maybe-unsupported
    -Wno-deprecated-copy-with-dtor
    -Wno-deprecated-copy-with-user-provided-dtor
    -Wno-disabled-macro-expansion
    -Wno-documentation
    -Wno-documentation-unknown-command
    -Wno-missing-prototypes
    -Wno-padded
    -Wno-reserved-macro-identifier
    -Wno-sign-conversion
    -Wno-shadow
    -Wno-shadow-field
    -Wno-shadow-field-in-constructor
    -Wno-switch-default
    -Wno-switch-enum
    -Wno-unneeded-member-function
    -Wno-unsafe-buffer-usage
    -Wno-unused-function
    -Wno-unused-member-function
    -Wno-unused-template
    -Wno-weak-vtables
)

# specify lib options
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(lib PUBLIC
        $<$<NOT:$<BOOL:$<TARGET_PROPERTY:DISABLE_STRICT_ALIASING>>>:-fstrict-aliasing>
        $<$<NOT:$<BOOL:$<TARGET_PROPERTY:DISABLE_STRICT_ALIASING>>>:-Wstrict-aliasing=2>

        $<$<COMPILE_LANGUAGE:CXX>:
            -Wdouble-promotion
            -Wfloat-conversion

            -fsized-deallocation

            ${crv_shared_cxx_options}
        >
    )

    target_link_options(lib PUBLIC
        "LINKER:-z,relro"
        "LINKER:-z,now"
    )
endif()
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(lib PUBLIC
        $<$<COMPILE_LANGUAGE:CXX>:
            ${crv_gnu_shared_cxx_options}
        >
    )
endif()
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(lib PUBLIC
        $<$<COMPILE_LANGUAGE:CXX>:
            -Wno-exit-time-destructors
            -Wno-float-equal
            -Wno-global-constructors

            ${crv_clang_shared_cxx_options}
        >
    )
endif()

target_include_directories(lib PUBLIC "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>")
target_link_libraries(lib tomlplusplus::tomlplusplus)

option(ENABLE_ASAN "Enable AddressSanitizer (ASan) for debug builds." OFF)
if (ENABLE_ASAN)
    target_compile_options(lib PUBLIC -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(lib PUBLIC link_options_user_mode -fsanitize=address)
endif()

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
        pipeline/filters/one_euro_integration_test.cpp
        pipeline/filters/one_euro/cutoff_interval_test.cpp
        pipeline/filters/one_euro/cutoff_rate_test.cpp
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
        spline/construction/curve_target_test.cpp
        spline/construction/segment/amr/approximant_test.cpp
        spline/construction/segment/amr/bisection_test.cpp
        spline/construction/segment/amr/error_metric_test.cpp
        spline/construction/segment/amr/interval_test.cpp
        spline/construction/segment/amr/node_generator_test.cpp
        spline/construction/segment/amr/residual_estimator_test.cpp
        spline/construction/segment/amr/subdivision_predicate_test.cpp
        spline/construction/segment/amr/subdivision_test.cpp
        spline/construction/segment/field_packer_test.cpp
        spline/construction/segment/local_coordinate_test.cpp
        spline/construction/segment/segment_factory_test.cpp
        spline/construction/segment/segment_packer_test.cpp
        spline/construction/segment/segment_quantizer_test.cpp
        spline/construction/segment/shift_planner_test.cpp
        spline/construction/spline/amr/assembler_test.cpp
        spline/construction/spline/amr/refinement_pool_seeder_test.cpp
        spline/construction/spline/amr/refiner_test.cpp
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

add_subdirectory(kernel)
add_subdirectory(ui)
