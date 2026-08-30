// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "domain_warp_curve.hpp"
#include <crv/model/curves/power_law.hpp>
#include <crv/model/domain.hpp>
#include <crv/model/shaping/input_affine_curve.hpp>
#include <crv/model/shaping/transforms/domain_warp.hpp>
#include <crv/model/shaping/transforms/input_affine.hpp>
#include <crv/model/shaping/transitions/construction/transition_factory_builder.hpp>
#include <crv/model/shaping/transitions/continuity.hpp>
#include <crv/model/shaping/transitions/smoothstep.hpp>
#include <crv/quadrature/antiderivative_factory.hpp>
#include <crv/test/test.hpp>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

namespace crv::shaping {
namespace {

using scalar_t = float_t;
using jet_t = crv::jet_t<scalar_t>;
using smoothstep_t = transitions::smoothstep_t;
using warp_t = transforms::domain_warp_t<scalar_t, smoothstep_t>;
using power_law_t = model::curves::power_law_t;
using power_law_evaluator_t = power_law_t::evaluator_t<scalar_t>;

[[nodiscard]] auto make_warp(scalar_t hold_width, scalar_t transition_width) -> warp_t
{
    return std::move(warp_t::make(hold_width, transition_width, smoothstep_t{})).value();
}

TEST(domain_warp_power_law_integration_test_t, exact_hold_avoids_fractional_power_law_singular_jet)
{
    auto const raw = power_law_evaluator_t{power_law_t::params_t<scalar_t>{1.0, 0.5}};
    auto const sut = domain_warp_curve_t<warp_t, power_law_evaluator_t>{make_warp(1.0, 1.0), raw};
    EXPECT_EQ(sut(jet_t{0.5, 1.0}), (jet_t{0.0, 0.0}));
}

TEST(domain_warp_power_law_integration_test_t, transition_release_resumes_ordinary_jet_composition)
{
    auto const raw = power_law_evaluator_t{power_law_t::params_t<scalar_t>{1.0, 0.5}};
    auto const warp = make_warp(1.0, 1.0);
    auto const sut = domain_warp_curve_t<warp_t, power_law_evaluator_t>{warp, raw};
    auto const input = jet_t{1.5, 1.0};
    EXPECT_EQ(sut(input), raw(warp.apply(input)));
}

TEST(domain_warp_power_law_integration_test_t, transition_release_has_finite_tangent_at_representative_interior_point)
{
    auto const raw = power_law_evaluator_t{power_law_t::params_t<scalar_t>{1.0, 0.5}};
    auto const sut = domain_warp_curve_t<warp_t, power_law_evaluator_t>{make_warp(1.0, 1.0), raw};
    EXPECT_TRUE(std::isfinite(sut(jet_t{1.5, 1.0}).df));
}

using affine_transform_t = transforms::input_affine_t<scalar_t>;
using affine_power_law_t = input_affine_curve_t<affine_transform_t, power_law_evaluator_t>;
using shaped_power_law_t = domain_warp_curve_t<warp_t, affine_power_law_t>;

static_assert(is_curve<shaped_power_law_t, scalar_t>);

[[nodiscard]] auto make_warp_affine_power_law() -> shaped_power_law_t
{
    auto const raw = power_law_evaluator_t{power_law_t::params_t<scalar_t>{1.0, 0.5}};
    auto affine = affine_power_law_t{affine_transform_t::make(1.0, -0.25).value(), raw};
    return {make_warp(1.0, 0.0), std::move(affine)};
}

TEST(
    domain_warp_input_affine_power_law_integration_test_t, enclosing_hold_maps_physical_input_to_affine_zero_coordinate)
{
    EXPECT_EQ(make_warp_affine_power_law()(-100.0), 0.5);
}

TEST(domain_warp_input_affine_power_law_integration_test_t, enclosing_hold_extends_domain_only_when_affine_zero_is_safe)
{
    EXPECT_TRUE(make_warp_affine_power_law().domain().contains(-std::numeric_limits<scalar_t>::max()));
}

TEST(domain_warp_input_affine_power_law_integration_test_t, post_hold_evaluation_preserves_warp_then_affine_order)
{
    auto const raw = power_law_evaluator_t{power_law_t::params_t<scalar_t>{1.0, 0.5}};
    EXPECT_EQ(make_warp_affine_power_law()(1.5), raw(0.75));
}

struct positive_domain_t
{
    [[nodiscard]] auto contains(scalar_t input) const noexcept -> bool
    {
        return std::isfinite(input) && input > scalar_t{0};
    }
};

struct positive_identity_curve_t
{
    using scalar_t = crv::float_t;
    using domain_t = positive_domain_t;

    [[nodiscard]] constexpr auto operator()(scalar_t input) const noexcept -> scalar_t { return input; }
    [[nodiscard]] constexpr auto domain() const noexcept -> domain_t { return {}; }
    [[nodiscard]] auto critical_points() const -> std::vector<scalar_t> { return {}; }
};

TEST(domain_warp_domain_integration_test_t, hold_is_excluded_when_nested_domain_rejects_zero)
{
    auto const sut = domain_warp_curve_t<warp_t, positive_identity_curve_t>{make_warp(1.0, 1.0), {}};
    EXPECT_FALSE(sut.domain().contains(0.5));
}

TEST(domain_warp_domain_integration_test_t, release_is_included_when_warp_maps_into_nested_domain)
{
    auto const sut = domain_warp_curve_t<warp_t, positive_identity_curve_t>{make_warp(1.0, 1.0), {}};
    EXPECT_TRUE(sut.domain().contains(3.0));
}

struct identity_curve_t
{
    using scalar_t = crv::float_t;
    using domain_t = model::unbounded_domain_t<scalar_t>;

    [[nodiscard]] constexpr auto operator()(scalar_t input) const noexcept -> scalar_t { return input; }
    [[nodiscard]] constexpr auto operator()(jet_t input) const noexcept -> jet_t { return input; }
    [[nodiscard]] constexpr auto domain() const noexcept -> domain_t { return {}; }
    [[nodiscard]] auto critical_points() const -> std::vector<scalar_t> { return {}; }
};

static_assert(is_curve<identity_curve_t, scalar_t>);

TEST(domain_warp_nast_integration_test_t, composes_with_retained_numerical_antiderivative)
{
    using antiderivative_factory_t = quadrature::antiderivative_factory_t<scalar_t>;
    using builder_t = transitions::construction::transition_factory_builder_t<antiderivative_factory_t>;

    auto const factory = builder_t{antiderivative_factory_t{}, scalar_t{1e-12}, int_t{32}}();
    auto const actual = factory(transitions::continuity_t::cinfinity, []<typename product_t>(product_t product) {
        using transition_t = product_t::transition_t;
        using transform_t = transforms::domain_warp_t<scalar_t, transition_t>;
        using curve_t = domain_warp_curve_t<transform_t, identity_curve_t>;

        auto transform = transform_t::make(1.0, 2.0, std::move(product.transition)).value();
        return curve_t{std::move(transform), identity_curve_t{}}(2.0);
    });

    EXPECT_NEAR(actual, 2.0 * 0.0688874741344636, 4e-14);
}

} // namespace
} // namespace crv::shaping
