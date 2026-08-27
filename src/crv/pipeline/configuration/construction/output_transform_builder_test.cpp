// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "output_transform_builder.hpp"
#include <crv/pipeline/output_transform.hpp>
#include <crv/spline/pipeline_config.hpp>
#include <crv/test/test.hpp>
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
            && transform.rows_are_orthogonal() && transform.determinant_is_positive();
    }
};

TEST_P(output_transform_builder_test_t, matrix_satisfies_validation_tolerances)
{
    auto const [degrees, anisotropy] = GetParam();

    EXPECT_TRUE(is_valid(sut_t{}(degrees, anisotropy)));
    EXPECT_TRUE(is_valid(sut_t{}(-degrees, anisotropy)));
}

constexpr float_t rotation_degrees[]
    = {0, 0.1, 45, 89.9, 90, 90.1, 135, 179.9, 180, 180.1, 225, 269.9, 270, 270.1, 315, 359.9};

INSTANTIATE_TEST_SUITE_P(degrees_and_anisotropy, output_transform_builder_test_t,
    Combine(ValuesIn(rotation_degrees), Values(float_t{0.001}, float_t{1}, float_t{1000})));

} // namespace
} // namespace crv::pipeline::configuration::construction
