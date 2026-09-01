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

[[nodiscard]] auto apply_scalar(scalar_t scale, scalar_t shift, scalar_t input) noexcept -> scalar_t
{
    if (scale >= scalar_t{1}) return scale * (input - shift);
    return scale * input - scale * shift;
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

TEST(shaping_transforms_input_affine_try_apply_test_t, accepts_forward_representability_boundary)
{
    auto const max = std::numeric_limits<scalar_t>::max();
    EXPECT_EQ(make_sut(2.0, 0.0).try_apply(max / 2.0), max);
}

TEST(shaping_transforms_input_affine_try_apply_test_t, rejects_forward_input_beyond_representability_boundary)
{
    auto const max = std::numeric_limits<scalar_t>::max();
    auto const boundary = max / 2.0;
    EXPECT_FALSE(make_sut(2.0, 0.0).try_apply(std::nextafter(boundary, max)));
}

struct shaping_transforms_input_affine_nonfinite_input_test_t : TestWithParam<scalar_t>
{};

TEST_P(shaping_transforms_input_affine_nonfinite_input_test_t, try_apply_rejects_input)
{
    EXPECT_FALSE(make_sut(2.0, 1.0).try_apply(GetParam()));
}

INSTANTIATE_TEST_SUITE_P(nonfinite_inputs, shaping_transforms_input_affine_nonfinite_input_test_t,
    Values(std::numeric_limits<scalar_t>::infinity(), -std::numeric_limits<scalar_t>::infinity(),
        std::numeric_limits<scalar_t>::quiet_NaN()));

TEST(shaping_transforms_input_affine_try_apply_test_t, accepts_actual_finite_expanding_expression)
{
    auto const max = std::numeric_limits<scalar_t>::max();
    auto const input = -std::numeric_limits<scalar_t>::denorm_min();
    auto const expected = apply_scalar(1.0, max, input);
    ASSERT_TRUE(std::isfinite(expected));
    EXPECT_EQ(make_sut(1.0, max).try_apply(input), expected);
}

TEST(shaping_transforms_input_affine_try_apply_test_t, rejects_rounded_division_guard_false_positive)
{
    auto const max = std::numeric_limits<scalar_t>::max();
    auto const input = max / scalar_t{3};
    ASSERT_FALSE(std::isfinite(apply_scalar(3.0, 0.0, input)));
    EXPECT_FALSE(make_sut(3.0, 0.0).try_apply(input));
}

TEST(shaping_transforms_input_affine_try_apply_test_t, matches_actual_contracting_expression)
{
    auto const max = std::numeric_limits<scalar_t>::max();
    auto const input = max;
    auto const expected = apply_scalar(0.5, -max, input);
    ASSERT_TRUE(std::isfinite(expected));
    EXPECT_EQ(make_sut(0.5, -max).try_apply(input), expected);
}

TEST(shaping_transforms_input_affine_preimage_test_t, maps_ordinary_interval)
{
    auto const sut = make_sut(2.0, 1.0);
    auto const nested = model::input_domain_t<scalar_t>{-4.0, 10.0};
    auto const domain = sut.preimage(nested);
    auto const predecessor = std::nextafter(domain.first(), -std::numeric_limits<scalar_t>::infinity());
    auto const successor = std::nextafter(domain.last(), std::numeric_limits<scalar_t>::infinity());

    EXPECT_TRUE(nested.contains(apply_scalar(2.0, 1.0, domain.first())));
    EXPECT_FALSE(nested.contains(apply_scalar(2.0, 1.0, predecessor)));
    EXPECT_TRUE(nested.contains(apply_scalar(2.0, 1.0, domain.last())));
    EXPECT_FALSE(nested.contains(apply_scalar(2.0, 1.0, successor)));
}

TEST(shaping_transforms_input_affine_preimage_test_t, expanding_scale_resolves_exact_interval)
{
    EXPECT_EQ(make_sut(4.0, 0.0).preimage({-8.0, 12.0}), (model::input_domain_t<scalar_t>{-2.0, 3.0}));
}

TEST(shaping_transforms_input_affine_preimage_test_t, contracting_scale_resolves_exact_interval)
{
    EXPECT_EQ(make_sut(0.5, 0.0).preimage({-2.0, 3.0}), (model::input_domain_t<scalar_t>{-4.0, 6.0}));
}

TEST(shaping_transforms_input_affine_preimage_test_t, supports_negative_outer_interval)
{
    auto const domain = make_sut(2.0, 1.0).preimage({-4.0, -3.0});
    EXPECT_LT(domain.last(), 0.0);
}

TEST(shaping_transforms_input_affine_preimage_test_t, preserves_empty_nested_domain)
{
    EXPECT_TRUE(make_sut(2.0, 1.0).preimage(model::input_domain_t<scalar_t>{}).empty());
}

TEST(shaping_transforms_input_affine_preimage_test_t, exact_lower_boundary_uses_actual_forward_rounding)
{
    auto const max = std::numeric_limits<scalar_t>::max();
    auto const sut = make_sut(1.0, max);
    auto const domain = sut.preimage(model::input_domain_t<scalar_t>::full());
    auto const predecessor = std::nextafter(domain.first(), -std::numeric_limits<scalar_t>::infinity());

    EXPECT_TRUE(std::isfinite(apply_scalar(1.0, max, domain.first())));
    EXPECT_FALSE(std::isfinite(apply_scalar(1.0, max, predecessor)));
}

TEST(shaping_transforms_input_affine_preimage_test_t, exact_upper_boundary_uses_actual_forward_rounding)
{
    auto const max = std::numeric_limits<scalar_t>::max();
    auto const sut = make_sut(1.0, -max);
    auto const domain = sut.preimage(model::input_domain_t<scalar_t>::full());
    auto const successor = std::nextafter(domain.last(), std::numeric_limits<scalar_t>::infinity());

    EXPECT_TRUE(std::isfinite(apply_scalar(1.0, -max, domain.last())));
    EXPECT_FALSE(std::isfinite(apply_scalar(1.0, -max, successor)));
}

TEST(shaping_transforms_input_affine_preimage_test_t, contracting_expression_can_limit_forward_representability)
{
    auto const max = std::numeric_limits<scalar_t>::max();
    auto const sut = make_sut(0.75, max);
    auto const domain = sut.preimage(model::input_domain_t<scalar_t>::full());
    auto const predecessor = std::nextafter(domain.first(), -std::numeric_limits<scalar_t>::infinity());

    EXPECT_TRUE(std::isfinite(apply_scalar(0.75, max, domain.first())));
    EXPECT_FALSE(std::isfinite(apply_scalar(0.75, max, predecessor)));
}

TEST(shaping_transforms_input_affine_preimage_test_t, corrects_algebraic_inverse_that_rounds_outside_nested_upper)
{
    auto const sut = make_sut(0.1, 0.0);
    auto const nested = model::input_domain_t<scalar_t>{0.0, 1.7};
    auto const domain = sut.preimage(nested);
    auto const successor = std::nextafter(domain.last(), std::numeric_limits<scalar_t>::infinity());

    EXPECT_LT(domain.last(), 17.0);
    EXPECT_TRUE(nested.contains(apply_scalar(0.1, 0.0, domain.last())));
    EXPECT_FALSE(nested.contains(apply_scalar(0.1, 0.0, successor)));
}

TEST(shaping_transforms_input_affine_preimage_test_t, skipped_singleton_output_has_empty_preimage)
{
    auto const denorm = std::numeric_limits<scalar_t>::denorm_min();
    EXPECT_TRUE(make_sut(2.0, 0.0).preimage({denorm, denorm}).empty());
}

TEST(shaping_transforms_input_affine_preimage_test_t,
    adjacent_reachable_output_after_skipped_value_has_singleton_preimage)
{
    auto const denorm = std::numeric_limits<scalar_t>::denorm_min();
    EXPECT_EQ(make_sut(2.0, 0.0).preimage({denorm, 2.0 * denorm}), (model::input_domain_t<scalar_t>{denorm, denorm}));
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

TEST(shaping_transforms_input_affine_inverse_test_t, rejects_finite_rounded_candidate_outside_forward_preimage)
{
    auto const max = std::numeric_limits<scalar_t>::max();
    EXPECT_FALSE(make_sut(1.0, max).try_inverse(1.0));
}

TEST(shaping_transforms_input_affine_inverse_test_t, rejects_actual_nonfinite_shift_addition_at_predicted_boundary)
{
    auto const max = std::numeric_limits<scalar_t>::max();
    auto const shift = max * scalar_t{0.26};
    auto const input = max - shift;
    EXPECT_FALSE(make_sut(1.0, shift).try_inverse(input));
}

} // namespace
} // namespace crv::shaping::transforms
