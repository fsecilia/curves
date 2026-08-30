// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "output_transform_builder.hpp"
#include <crv/pipeline/output_transform.hpp>
#include <crv/spline/pipeline_config.hpp>
#include <crv/test/test.hpp>
#include <array>
#include <tuple>

namespace crv::pipeline::configuration::construction {
namespace {

using output_transform_builder_test_case_t = std::tuple<float_t, float_t>;

struct output_transform_builder_test_t : TestWithParam<output_transform_builder_test_case_t>
{
    using gain_t = spline::prod_pipeline_config_t::y_t;
    using transform_t = output_transform_t<gain_t>;
    using sut_t = output_transform_builder_t<transform_t>;

    static constexpr auto is_valid(transform_t const& transform) noexcept -> bool
    {
        return transform.rotation_components_are_valid() && transform.anisotropy_components_are_valid()
            && transform.rotation_norm_is_valid() && transform.anisotropy_norm_is_valid()
            && transform.rows_are_orthogonal() && transform.determinant_is_positive()
            && transform.output_scale_is_valid();
    }
};

TEST_P(output_transform_builder_test_t, matrix_satisfies_validation_tolerances)
{
    auto const [degrees, anisotropy] = GetParam();

    EXPECT_TRUE(is_valid(sut_t{}(degrees, anisotropy, 26'000, 1000)));
    EXPECT_TRUE(is_valid(sut_t{}(-degrees, anisotropy, 26'000, 1000)));
}

struct output_scale_builder_test_case_t
{
    int_t input_dpi;
    int_t output_dpi;
    uint64_t expected_raw;
};

struct output_scale_builder_test_t : TestWithParam<output_scale_builder_test_case_t>
{
    using transform_t = output_transform_builder_test_t::transform_t;
    using sut_t = output_transform_builder_test_t::sut_t;
};

TEST_P(output_scale_builder_test_t, rounds_ratio_to_transform_scale)
{
    auto const& [input_dpi, output_dpi, expected_raw] = GetParam();
    EXPECT_EQ(sut_t{}(0.0, 1.0, input_dpi, output_dpi).output_scale, transform_t::scale_t::literal(expected_raw));
}

constexpr auto output_scale_cases = std::array{
    output_scale_builder_test_case_t{26'000, 800, 66'076'420},
    output_scale_builder_test_case_t{26'000, 1000, 82'595'525},
    output_scale_builder_test_case_t{800, 1000, 2'684'354'560},
};
INSTANTIATE_TEST_SUITE_P(dpi_ratios, output_scale_builder_test_t, ValuesIn(output_scale_cases));

constexpr float_t rotation_degrees[]
    = {0, 0.1, 45, 89.9, 90, 90.1, 135, 179.9, 180, 180.1, 225, 269.9, 270, 270.1, 315, 359.9};

INSTANTIATE_TEST_SUITE_P(degrees_and_anisotropy, output_transform_builder_test_t,
    Combine(ValuesIn(rotation_degrees), Values(float_t{0.001}, float_t{1}, float_t{1000})));

} // namespace
} // namespace crv::pipeline::configuration::construction
