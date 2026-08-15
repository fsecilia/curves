// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include <crv/lib.hpp>
#include <crv/math/fixed/float_conversions.hpp>
#include <crv/math/polynomial.hpp>
#include <crv/spline/construction/segment/field_packer.hpp>
#include <crv/spline/construction/segment/local_coordinate.hpp>
#include <crv/spline/construction/segment/segment_packer.hpp>
#include <crv/spline/construction/segment/segment_quantizer.hpp>
#include <crv/spline/construction/segment/shift_planner.hpp>
#include <crv/spline/pipeline_config.hpp>
#include <crv/spline/segment.hpp>
#include <crv/test/test.hpp>
#include <array>
#include <cmath>

namespace crv::spline {
namespace {

using scalar_t = float_t;

//
// dynamic segments
//

struct spline_dynamic_segment_test_t : Test
{
    static constexpr auto segment_layout = prod_pipeline_config.segment_layout;
    static constexpr auto intermediate_layout_max_shift = segment_layout.intermediate.max_shift();
    static constexpr auto final_layout_min_shift = segment_layout.final.min_shift();
    static constexpr auto final_layout_max_shift = segment_layout.final.max_shift();

    using x_t = prod_pipeline_config_t::x_t;
    using y_t = prod_pipeline_config_t::y_t;

    using traits_t = traits_t<unpacked_field_t<int_t>>;
    using mantissa_t = traits_t::mantissa_t;
    using unpacked_field_t = traits_t::unpacked_field_t;
    using packed_field_t = traits_t::packed_field_t;
    using packed_segment_t = traits_t::packed_segment_t;
    using unpacked_segment_t = traits_t::unpacked_segment_t;
    using field_unpacker_t = field_unpacker_t<unpacked_field_t>;
    using segment_unpacker_t
        = segment_unpacker_t<packed_segment_t, unpacked_segment_t, field_unpacker_t, segment_layout>;
    using segment_evaluator_t = segment_evaluator_t<traits_t, x_t, y_t>;
    using segment_t = segment_t<traits_t, x_t, segment_unpacker_t, segment_evaluator_t>;
    using float_extractor_t = float_extractor_t<float64_t>;
    using exponent_aligner_t = exponent_aligner_t<final_layout_min_shift, final_layout_max_shift>;
    using scaled_int_t = float_extractor_t::scaled_int_t;
    using radix_aligner_t = radix_aligner_t<unpacked_field_t, scaled_int_t, exponent_aligner_t{}>;
    using field_packer_t = field_packer_t<packed_field_t>;
    using mantissa_quantizer_t = mantissa_quantizer_t<mantissa_t>;
    using shift_planner_t = shift_planner_t<mantissa_t>;
    using segment_quantizer_t = segment_quantizer_t<unpacked_field_t, float_extractor_t, shift_planner_t,
        mantissa_quantizer_t, radix_aligner_t, intermediate_layout_max_shift, x_t, y_t::frac_bits>;
    using segment_packer_t = segment_packer_t<packed_segment_t, unpacked_segment_t, field_packer_t, segment_layout>;
    using cubic_t = cubic_t<scalar_t>;

    local_coordinate_converter_t<scalar_t> convert_local;
    segment_quantizer_t segment_quantizer;
    segment_packer_t segment_packer;
    segment_unpacker_t segment_unpacker;
    segment_evaluator_t segment_evaluator;

    auto test(cubic_t const& normalized, x_t width_fixed, x_t u_fixed, scalar_t tolerance = 1e-10) -> void
    {
        ASSERT_GT(width_fixed, x_t{0});
        ASSERT_GE(u_fixed, x_t{0});
        ASSERT_LE(u_fixed, width_fixed);

        auto const width = from_fixed<scalar_t>(width_fixed);
        auto const u = from_fixed<scalar_t>(u_fixed);
        auto const t = u / width;
        auto const local = convert_local(normalized, width);

        auto const normalized_oracle = normalized(t);
        auto const local_oracle = local(u);
        EXPECT_NEAR(normalized_oracle, local_oracle, 5e-13 * std::max(std::abs(normalized_oracle), scalar_t{1}));

        auto const quantized_segment = segment_quantizer(local, width_fixed);
        auto const packed_segment = segment_packer(quantized_segment);
        auto const unpacked_segment = segment_unpacker(packed_segment);
        auto const actual = from_fixed<scalar_t>(segment_evaluator(unpacked_segment, u_fixed));

        EXPECT_NEAR(normalized_oracle, actual, tolerance);
    }
};

static_assert(sizeof(typename spline_dynamic_segment_test_t::segment_t) == 32);
static_assert(alignof(typename spline_dynamic_segment_test_t::segment_t) == 32);

TEST_F(spline_dynamic_segment_test_t, arbitrary_width)
{
    auto const normalized = cubic_t{1.4, -2.0, 1.0, 0.1};
    auto const width = to_fixed<x_t>(0.3);

    test(normalized, width, x_t::literal(width.value / 3));
    test(normalized, width, x_t::literal((width.value * 2) / 3));
    test(normalized, width, width);
}

TEST_F(spline_dynamic_segment_test_t, odd_raw_width)
{
    auto const normalized = cubic_t{1.4, -2.0, 1.0, 0.1};
    auto const width = x_t::literal(12345);

    test(normalized, width, x_t::literal(1));
    test(normalized, width, x_t::literal(6172));
    test(normalized, width, width);
}

TEST_F(spline_dynamic_segment_test_t, extreme_supported_widths)
{
    auto const normalized = cubic_t{0.125, 0.25, 0.5, 1.0};

    auto const smallest = x_t::literal(1);
    test(normalized, smallest, x_t::literal(0));
    test(normalized, smallest, smallest, 2e-10);

    auto const largest = x_t{256};
    test(normalized, largest, largest / 2);
    test(normalized, largest, largest);
}

//
// dyadic segments
//

struct dyadic_vector_t
{
    scalar_t t;
    scalar_t expected;
};

struct spline_dynamic_segment_dyadic_test_t : spline_dynamic_segment_test_t, WithParamInterface<dyadic_vector_t>
{
    static constexpr auto normalized = cubic_t{1.4, -2.0, 1.0, 0.1};

    auto test_dyadic(int_t log2_width) -> void
    {
        auto const width_real = std::ldexp(1.0, static_cast<int>(log2_width));
        auto const width = to_fixed<x_t>(width_real);
        auto const u = to_fixed<x_t>(GetParam().t * width_real);

        EXPECT_NEAR(normalized(GetParam().t), GetParam().expected, 5e-13);
        test(normalized, width, u);
    }
};

TEST_P(spline_dynamic_segment_dyadic_test_t, agrees_with_normalized_dyadic_reference)
{
    for (auto const log2_width : std::array{-8, -1, 0, 1, 8}) test_dyadic(log2_width);
}

dyadic_vector_t const dyadic_vectors[] = {
    {0.0, 0.1},
    {0.25, 0.246875},
    {1.0 / 3.0, 0.262962962963},
    {0.5, 0.275},
    {2.0 / 3.0, 0.292592592593},
    {0.75, 0.315625},
    {1.0, 0.5},
};
INSTANTIATE_TEST_SUITE_P(dyadic_vectors, spline_dynamic_segment_dyadic_test_t, ValuesIn(dyadic_vectors));

//
// fixed-output tests
//

struct fixed_output_vector_t
{
    int_t log2_width;
    scalar_t t;
    int_t expected_raw;
};

struct spline_dynamic_segment_fixed_output_test_t : spline_dynamic_segment_test_t,
                                                    WithParamInterface<fixed_output_vector_t>
{};

TEST_P(spline_dynamic_segment_fixed_output_test_t, preserves_fixed_outputs)
{
    auto const normalized = cubic_t{1.4, -2.0, 1.0, 0.1};
    auto const& regression = GetParam();

    auto const width_real = std::ldexp(1.0, static_cast<int>(regression.log2_width));
    auto const width = to_fixed<x_t>(width_real);
    auto const u = to_fixed<x_t>(regression.t * width_real);
    auto const local = convert_local(normalized, width_real);
    auto const quantized_segment = segment_quantizer(local, width);
    auto const packed_segment = segment_packer(quantized_segment);
    auto const unpacked_segment = segment_unpacker(packed_segment);

    EXPECT_EQ(segment_evaluator(unpacked_segment, u).value, regression.expected_raw);
}

// exercise complete fixed evaluation path, including representability difference at t = 1/3 for narrowest interval
auto const fixed_output_vectors = std::array{
    fixed_output_vector_t{-8, 1.0 / 3.0, 9252186734482},
    fixed_output_vector_t{-1, 1.0 / 3.0, 9252186734471},
    fixed_output_vector_t{0, 1.0 / 3.0, 9252186734471},
    fixed_output_vector_t{1, 1.0 / 3.0, 9252186734471},
    fixed_output_vector_t{8, 1.0 / 3.0, 9252186734471},
    fixed_output_vector_t{-8, 0.75, 11105067440538},
    fixed_output_vector_t{8, 0.75, 11105067440538},
};
INSTANTIATE_TEST_SUITE_P(
    fixed_output_vectors, spline_dynamic_segment_fixed_output_test_t, ValuesIn(fixed_output_vectors));

} // namespace
} // namespace crv::spline
