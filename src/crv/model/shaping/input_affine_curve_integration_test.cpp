// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "input_affine_curve.hpp"
#include <crv/model/curves/power_law.hpp>
#include <crv/model/shaping/transforms/input_affine.hpp>
#include <crv/test/test.hpp>
#include <cmath>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

namespace crv::shaping {
namespace {

using scalar_t = float_t;
using jet_t = crv::jet_t<scalar_t>;
using transform_t = transforms::input_affine_t<scalar_t>;

struct interval_domain_t
{
    std::optional<scalar_t> left;
    std::optional<scalar_t> right;

    [[nodiscard]] auto contains(scalar_t input) const noexcept -> bool
    {
        if (!std::isfinite(input)) return false;
        if (left && input < *left) return false;
        if (right && input > *right) return false;
        return true;
    }
};

struct identity_curve_t
{
    using scalar_t = crv::float_t;
    using domain_t = interval_domain_t;

    domain_t input_domain{};
    std::vector<scalar_t> points{};

    [[nodiscard]] constexpr auto operator()(scalar_t input) const noexcept -> scalar_t { return input; }
    [[nodiscard]] constexpr auto operator()(jet_t input) const noexcept -> jet_t { return input; }
    [[nodiscard]] constexpr auto domain() const noexcept -> domain_t { return input_domain; }
    [[nodiscard]] auto critical_points() const -> std::vector<scalar_t> { return points; }
};

using sut_t = input_affine_curve_t<transform_t, identity_curve_t>;

[[nodiscard]] auto make_sut(
    scalar_t scale, scalar_t shift, interval_domain_t domain = {}, std::vector<scalar_t> points = {}) -> sut_t
{
    return {std::move(transform_t::make(scale, shift)).value(), identity_curve_t{domain, std::move(points)}};
}

TEST(input_affine_curve_integration_test_t, composes_real_transform_for_scalar)
{
    EXPECT_EQ(make_sut(3.0, 2.0)(5.0), 9.0);
}

TEST(input_affine_curve_integration_test_t, composes_real_transform_for_jet)
{
    EXPECT_EQ(make_sut(2.0, 3.0)(jet_t{5.0, 7.0}), (jet_t{4.0, 14.0}));
}

TEST(input_affine_curve_integration_test_t, nested_zero_left_bound_accepts_exactly_feasible_origin)
{
    auto const sut = make_sut(1.0, 0.0, {.left = 0.0, .right = std::nullopt});
    EXPECT_TRUE(sut.domain().contains(0.0));
}

TEST(input_affine_curve_integration_test_t, nested_zero_left_bound_rejects_immediately_infeasible_origin)
{
    auto const shift = std::numeric_limits<scalar_t>::denorm_min();
    auto const sut = make_sut(1.0, shift, {.left = 0.0, .right = std::nullopt});
    EXPECT_FALSE(sut.domain().contains(0.0));
}

TEST(input_affine_curve_integration_test_t, finite_negative_left_bound_inverse_maps_exactly)
{
    auto const sut = make_sut(2.0, 1.0, {.left = -4.0, .right = std::nullopt});
    EXPECT_TRUE(sut.domain().contains(-1.0));
}

TEST(input_affine_curve_integration_test_t, finite_negative_left_bound_rejects_value_below_inverse)
{
    auto const sut = make_sut(2.0, 1.0, {.left = -4.0, .right = std::nullopt});
    EXPECT_FALSE(sut.domain().contains(-1.25));
}

TEST(input_affine_curve_integration_test_t, unbounded_negative_extent_survives_contracting_scale)
{
    auto const sut = make_sut(0.5, 0.0, {.left = std::nullopt, .right = 10.0});
    EXPECT_TRUE(sut.domain().contains(std::numeric_limits<scalar_t>::lowest()));
}

TEST(input_affine_curve_integration_test_t, unbounded_positive_extent_observes_affine_representability)
{
    auto const sut = make_sut(2.0, 0.0);
    auto const max = std::numeric_limits<scalar_t>::max();
    auto const safe_right = max / 2.0;
    EXPECT_FALSE(sut.domain().contains(std::nextafter(safe_right, max)));
}

TEST(input_affine_curve_integration_test_t, finite_positive_nested_extent_inverse_maps_exactly)
{
    auto const sut = make_sut(2.0, 1.0, {.left = std::nullopt, .right = 10.0});
    EXPECT_TRUE(sut.domain().contains(6.0));
}

TEST(input_affine_curve_integration_test_t, finite_positive_nested_extent_rejects_value_above_inverse)
{
    auto const sut = make_sut(2.0, 1.0, {.left = std::nullopt, .right = 10.0});
    EXPECT_FALSE(sut.domain().contains(std::nextafter(6.0, std::numeric_limits<scalar_t>::infinity())));
}

TEST(input_affine_curve_integration_test_t, inverse_maps_multiple_critical_points)
{
    auto const sut = make_sut(2.0, 1.0, {}, {2.0, 6.0});
    EXPECT_EQ(sut.critical_points(), (std::vector<scalar_t>{2.0, 4.0}));
}

TEST(input_affine_power_law_integration_test_t, propagates_power_law_positive_representability)
{
    using power_law_t = model::curves::power_law_t;
    using evaluator_t = power_law_t::evaluator_t<scalar_t>;
    using sut_t = input_affine_curve_t<transform_t, evaluator_t>;

    auto const raw = evaluator_t{power_law_t::params_t<scalar_t>{1.0, 256.0}};
    auto const transform = transform_t::make(2.0, 1.0).value();
    auto const sut = sut_t{transform, raw};

    EXPECT_FALSE(sut.domain().contains(std::nextafter(9.0, std::numeric_limits<scalar_t>::infinity())));
}

TEST(input_affine_power_law_integration_test_t, preserves_power_law_left_scalar_boundary)
{
    using power_law_t = model::curves::power_law_t;
    using evaluator_t = power_law_t::evaluator_t<scalar_t>;
    using sut_t = input_affine_curve_t<transform_t, evaluator_t>;

    auto const raw = evaluator_t{power_law_t::params_t<scalar_t>{1.0, 0.5}};
    auto const transform = transform_t::make(2.0, 1.0).value();
    auto const sut = sut_t{transform, raw};

    EXPECT_TRUE(sut.domain().contains(1.0));
}

} // namespace
} // namespace crv::shaping
