// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "input_affine.hpp"
#include <crv/test/test.hpp>
#include <cmath>
#include <limits>
#include <ostream>
#include <string_view>
#include <utility>

namespace crv::shaping::transforms {
namespace {

using scalar_t = float_t;
using jet_t = crv::jet_t<scalar_t>;
using sut_t = input_affine_t<scalar_t>;

[[nodiscard]] auto make_sut(scalar_t scale, scalar_t shift) -> sut_t
{
    return std::move(sut_t::make(scale, shift)).value();
}

struct scalar_mapping_t
{
    std::string_view name;
    scalar_t scale;
    scalar_t shift;
    scalar_t input;
    scalar_t expected;

    friend auto operator<<(std::ostream& out, scalar_mapping_t const& value) -> std::ostream&
    {
        return out << value.name;
    }
};

struct shaping_transforms_input_affine_scalar_test_t : TestWithParam<scalar_mapping_t>
{};

TEST_P(shaping_transforms_input_affine_scalar_test_t, maps_input)
{
    auto const& param = GetParam();
    EXPECT_EQ(make_sut(param.scale, param.shift).apply(param.input), param.expected);
}

INSTANTIATE_TEST_SUITE_P(mapping, shaping_transforms_input_affine_scalar_test_t,
    Values(scalar_mapping_t{"scale_only", 2.0, 0.0, 3.0, 6.0}, scalar_mapping_t{"translation_only", 1.0, 2.0, 5.0, 3.0},
        scalar_mapping_t{"combined", 3.0, 2.0, 5.0, 9.0}));

TEST(shaping_transforms_input_affine_scalar_test_t, contracting_scale_avoids_overflow_before_cancellation)
{
    auto const max = std::numeric_limits<scalar_t>::max();
    EXPECT_EQ(make_sut(0.5, -max).apply(max), max);
}

TEST(shaping_transforms_input_affine_scalar_test_t, expanding_scale_avoids_distributed_product_overflow_near_shift)
{
    auto const max = std::numeric_limits<scalar_t>::max();
    EXPECT_EQ(make_sut(2.0, max).apply(max), 0.0);
}

TEST(shaping_transforms_input_affine_jet_test_t, maps_primal_and_tangent)
{
    EXPECT_EQ(make_sut(2.0, 3.0).apply(jet_t{5.0, 7.0}), (jet_t{4.0, 14.0}));
}

struct invalid_affine_t
{
    std::string_view name;
    scalar_t scale;
    scalar_t shift;
    input_affine_error_t error;

    friend auto operator<<(std::ostream& out, invalid_affine_t const& value) -> std::ostream&
    {
        return out << value.name;
    }
};

struct shaping_transforms_input_affine_parameter_test_t : TestWithParam<invalid_affine_t>
{};

TEST_P(shaping_transforms_input_affine_parameter_test_t, rejects_invalid_parameters)
{
    auto const& param = GetParam();
    EXPECT_EQ(sut_t::make(param.scale, param.shift), std::unexpected{param.error});
}

INSTANTIATE_TEST_SUITE_P(parameters, shaping_transforms_input_affine_parameter_test_t,
    Values(invalid_affine_t{"zero_scale", 0.0, 0.0, input_affine_error_t::scale_not_positive},
        invalid_affine_t{"negative_scale", -1.0, 0.0, input_affine_error_t::scale_not_positive},
        invalid_affine_t{
            "nan_scale", std::numeric_limits<scalar_t>::quiet_NaN(), 0.0, input_affine_error_t::scale_not_finite},
        invalid_affine_t{
            "infinite_scale", std::numeric_limits<scalar_t>::infinity(), 0.0, input_affine_error_t::scale_not_finite},
        invalid_affine_t{
            "nan_shift", 1.0, std::numeric_limits<scalar_t>::quiet_NaN(), input_affine_error_t::shift_not_finite},
        invalid_affine_t{
            "infinite_shift", 1.0, std::numeric_limits<scalar_t>::infinity(), input_affine_error_t::shift_not_finite}));

TEST(shaping_transforms_input_affine_domain_test_t, accepts_forward_representability_boundary)
{
    auto const max = std::numeric_limits<scalar_t>::max();
    EXPECT_EQ(make_sut(2.0, 0.0).try_apply(max / 2.0), max);
}

TEST(shaping_transforms_input_affine_domain_test_t, rejects_forward_input_beyond_representability_boundary)
{
    auto const max = std::numeric_limits<scalar_t>::max();
    auto const boundary = max / 2.0;
    EXPECT_FALSE(make_sut(2.0, 0.0).try_apply(std::nextafter(boundary, max)));
}

TEST(shaping_transforms_input_affine_inverse_test_t, inverse_maps_input)
{
    EXPECT_EQ(make_sut(2.0, 1.0).try_inverse(6.0), 4.0);
}

TEST(shaping_transforms_input_affine_inverse_test_t, contracting_scale_rejects_unrepresentable_quotient)
{
    auto const max = std::numeric_limits<scalar_t>::max();
    EXPECT_FALSE(make_sut(0.5, 0.0).try_inverse(max));
}

TEST(shaping_transforms_input_affine_inverse_test_t, rejects_unrepresentable_shift_addition)
{
    auto const max = std::numeric_limits<scalar_t>::max();
    EXPECT_FALSE(make_sut(1.0, max).try_inverse(1.0));
}

} // namespace
} // namespace crv::shaping::transforms
