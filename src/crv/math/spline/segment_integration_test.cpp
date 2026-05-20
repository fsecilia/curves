// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include <crv/lib.hpp>
#include <crv/math/polynomial.hpp>
#include <crv/math/spline/construction/segment/field_packer.hpp>
#include <crv/math/spline/construction/segment/quantization/mantissa_quantizer.hpp>
#include <crv/math/spline/construction/segment/quantization/radix_aligner.hpp>
#include <crv/math/spline/construction/segment/quantization/segment_quantizer.hpp>
#include <crv/math/spline/construction/segment/quantization/shift_planner.hpp>
#include <crv/math/spline/construction/segment/segment_factory.hpp>
#include <crv/math/spline/construction/segment/segment_packer.hpp>
#include <crv/math/spline/pipeline_config.hpp>
#include <crv/math/spline/segment.hpp>
#include <crv/test/test.hpp>

namespace crv::spline {
namespace {

// --------------------------------------------------------------------------------------------------------------------
// testing
// --------------------------------------------------------------------------------------------------------------------

using scalar_t = float_t;

struct spline_dynamic_segment_test_t : Test
{
    static constexpr auto log2_min_width = -16;
    static constexpr auto segment_layout = prod_pipeline_config.segment_layout;
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
    using field_layout_t = field_layout_t<packed_field_t>;
    using field_unpacker_t = field_unpacker_t<unpacked_field_t>;
    using segment_unpacker_t
        = segment_unpacker_t<packed_segment_t, unpacked_segment_t, field_unpacker_t, segment_layout>;
    using segment_evaluator_t = segment_evaluator_t<traits_t, x_t, y_t>;
    using float_extractor_t = float_extractor_t<float64_t>;
    using exponent_aligner_t = exponent_aligner_t<final_layout_min_shift, final_layout_max_shift>;
    using scaled_int_t = float_extractor_t::scaled_int_t;
    using radix_aligner_t = radix_aligner_t<unpacked_field_t, scaled_int_t, exponent_aligner_t{}>;
    using field_packer_t = field_packer_t<packed_field_t>;
    using mantissa_quantizer_t = mantissa_quantizer_t<mantissa_t>;
    using segment_quantizer_t = segment_quantizer_t<unpacked_field_t, float_extractor_t, shift_planner_t,
        mantissa_quantizer_t, radix_aligner_t, x_t::frac_bits, y_t::frac_bits, log2_min_width>;
    using segment_packer_t = segment_packer_t<packed_segment_t, unpacked_segment_t, field_packer_t, segment_layout>;
    using cubic_t = cubic_t<scalar_t>;

    segment_quantizer_t segment_quantizer;
    segment_packer_t segment_packer;
    segment_unpacker_t segment_unpacker;
    segment_evaluator_t segment_evaluator;

    auto test(cubic_t const& cubic, int_t log2_width, scalar_t input, scalar_t expected) -> void
    {
        // double check float value
        auto const t = input;
        auto const oracle = cubic(t);
        EXPECT_NEAR(expected, oracle, 5e-13);

        auto const quantized_segment = segment_quantizer(cubic, log2_width);
        auto const packed_segment = segment_packer(quantized_segment);
        auto const unpacked_segment = segment_unpacker(packed_segment);

        // check actual result
        auto const width = std::ldexp(1.0, static_cast<int>(log2_width));
        auto const dx = to_fixed<x_t>(input * width);
        auto const actual_fixed = segment_evaluator(unpacked_segment, dx);
        auto const actual_float = from_fixed<scalar_t>(actual_fixed);
        EXPECT_NEAR(expected, actual_float, 1e-10);
    }
};

TEST_F(spline_dynamic_segment_test_t, pathological_integral)
{
    auto const cubic = cubic_t{0.0, 0.0, 1000.0, 0.0};
    test(cubic, 8, 256, 256 * 1000);
};

// --------------------------------------------------------------------------------------------------------------------
// parameterized tests
// --------------------------------------------------------------------------------------------------------------------

struct vector_t
{
    scalar_t input;
    scalar_t expected;

    friend auto operator<<(std::ostream& out, vector_t const& src) -> std::ostream&
    {
        return out << "{.input = " << src.input << ", .expected = " << src.expected << "}";
    }
};

struct spline_dynamic_segment_param_test_t : spline_dynamic_segment_test_t, WithParamInterface<vector_t>
{
    scalar_t const input = GetParam().input;
    scalar_t const expected = GetParam().expected;

    // hermite: p0 = 0.1, m0 = 1, p1 = 0.5, m1 = 1.2
    static constexpr auto cubic = cubic_t{1.4, -2.0, 1.0, 0.1};

    auto test(int_t log2_width) -> void { spline_dynamic_segment_test_t::test(cubic, log2_width, input, expected); }
};

TEST_P(spline_dynamic_segment_param_test_t, log2_width_m8)
{
    test(-8);
}

TEST_P(spline_dynamic_segment_param_test_t, log2_width_m1)
{
    test(-1);
}

TEST_P(spline_dynamic_segment_param_test_t, log2_width_0)
{
    test(0);
}

TEST_P(spline_dynamic_segment_param_test_t, log2_width_1)
{
    test(1);
}

TEST_P(spline_dynamic_segment_param_test_t, log2_width_8)
{
    test(8);
}

vector_t const vectors[] = {
    {0.0 / 1.0, 0.1},
    {1.0 / 4.0, 0.246875},
    {1.0 / 3.0, 0.262962962963},
    {1.0 / 2.0, 0.275},
    {2.0 / 3.0, 0.292592592593},
    {3.0 / 4.0, 0.315625},
    {1.0 / 1.0, 0.5},
};
INSTANTIATE_TEST_SUITE_P(vectors, spline_dynamic_segment_param_test_t, ValuesIn(vectors));

} // namespace
} // namespace crv::spline
