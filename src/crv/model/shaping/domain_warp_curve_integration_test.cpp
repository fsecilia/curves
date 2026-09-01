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
using input_domain_t = model::input_domain_t<scalar_t>;
using smoothstep_t = transitions::smoothstep_t;
using warp_t = transforms::domain_warp_t<scalar_t, smoothstep_t>;
using power_law_t = model::curves::power_law_t;
using power_law_evaluator_t = power_law_t::evaluator_t<scalar_t>;

[[nodiscard]] auto make_warp(scalar_t hold_width, scalar_t transition_width) -> warp_t
{
    return std::move(warp_t::make(hold_width, transition_width, smoothstep_t{})).value();
}

struct identity_curve_t
{
    using scalar_t = crv::float_t;

    input_domain_t domain{input_domain_t::full()};

    [[nodiscard]] constexpr auto operator()(scalar_t input) const noexcept -> scalar_t { return input; }
    [[nodiscard]] constexpr auto operator()(jet_t input) const noexcept -> jet_t { return input; }
    [[nodiscard]] constexpr auto input_domain() const noexcept -> input_domain_t { return domain; }
    [[nodiscard]] auto critical_points() const -> std::vector<scalar_t> { return {}; }
};

using identity_warp_curve_t = domain_warp_curve_t<warp_t, identity_curve_t>;

[[nodiscard]] auto make_domain_sut(input_domain_t nested_domain) -> identity_warp_curve_t
{
    return {make_warp(2.0, 4.0), identity_curve_t{nested_domain}};
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

TEST(domain_warp_input_affine_power_law_integration_test_t, enclosing_hold_extends_domain_when_affine_zero_is_safe)
{
    EXPECT_EQ(make_warp_affine_power_law().input_domain().first(), std::numeric_limits<scalar_t>::lowest());
}

TEST(domain_warp_input_affine_power_law_integration_test_t, post_hold_evaluation_preserves_warp_then_affine_order)
{
    auto const raw = power_law_evaluator_t{power_law_t::params_t<scalar_t>{1.0, 0.5}};
    EXPECT_EQ(make_warp_affine_power_law()(1.5), raw(0.75));
}

TEST(domain_warp_domain_integration_test_t, full_nested_domain_keeps_full_finite_outer_domain)
{
    EXPECT_EQ(make_domain_sut(input_domain_t::full()).input_domain(), input_domain_t::full());
}

TEST(domain_warp_domain_integration_test_t, nested_domain_below_zero_has_empty_preimage)
{
    EXPECT_TRUE(make_domain_sut({-2.0, -1.0}).input_domain().empty());
}

TEST(domain_warp_domain_integration_test_t, nested_zero_only_domain_ends_at_exact_hold_release_frontier)
{
    auto const warp = make_warp(2.0, 4.0);
    auto const domain = make_domain_sut({0.0, 0.0}).input_domain();
    auto const successor = std::nextafter(domain.last(), std::numeric_limits<scalar_t>::infinity());

    EXPECT_EQ(domain.first(), std::numeric_limits<scalar_t>::lowest());
    EXPECT_EQ(warp.apply(domain.last()), 0.0);
    EXPECT_GT(warp.apply(successor), 0.0);
}

TEST(domain_warp_domain_integration_test_t, nested_first_zero_keeps_full_negative_hold_extent)
{
    EXPECT_EQ(make_domain_sut({0.0, 0.375}).input_domain().first(), std::numeric_limits<scalar_t>::lowest());
}

TEST(domain_warp_domain_integration_test_t, positive_nested_first_excludes_hold_at_exact_representable_boundary)
{
    auto const warp = make_warp(2.0, 4.0);
    auto const nested = input_domain_t{std::numeric_limits<scalar_t>::denorm_min(), 2.0};
    auto const domain = make_domain_sut(nested).input_domain();
    auto const predecessor = std::nextafter(domain.first(), -std::numeric_limits<scalar_t>::infinity());

    EXPECT_TRUE(nested.contains(warp.apply(domain.first())));
    EXPECT_FALSE(nested.contains(warp.apply(predecessor)));
}

TEST(domain_warp_domain_integration_test_t, resolves_lower_endpoint_inside_smooth_transition)
{
    auto const warp = make_warp(2.0, 4.0);
    auto const nested = input_domain_t{0.375, 4.0};
    auto const domain = make_domain_sut(nested).input_domain();
    auto const predecessor = std::nextafter(domain.first(), -std::numeric_limits<scalar_t>::infinity());

    EXPECT_TRUE(nested.contains(warp.apply(domain.first())));
    EXPECT_FALSE(nested.contains(warp.apply(predecessor)));
}

TEST(domain_warp_domain_integration_test_t, resolves_upper_endpoint_inside_smooth_transition)
{
    auto const warp = make_warp(2.0, 4.0);
    auto const nested = input_domain_t{0.0, 0.375};
    auto const domain = make_domain_sut(nested).input_domain();
    auto const successor = std::nextafter(domain.last(), std::numeric_limits<scalar_t>::infinity());

    EXPECT_TRUE(nested.contains(warp.apply(domain.last())));
    EXPECT_FALSE(nested.contains(warp.apply(successor)));
}

TEST(domain_warp_domain_integration_test_t, resolves_upper_endpoint_at_release)
{
    auto const warp = make_warp(2.0, 4.0);
    auto const nested = input_domain_t{0.0, 2.0};
    auto const domain = make_domain_sut(nested).input_domain();
    auto const successor = std::nextafter(domain.last(), std::numeric_limits<scalar_t>::infinity());

    EXPECT_TRUE(nested.contains(warp.apply(domain.last())));
    EXPECT_FALSE(nested.contains(warp.apply(successor)));
}

TEST(domain_warp_domain_integration_test_t, resolves_upper_endpoint_after_release)
{
    auto const warp = make_warp(2.0, 4.0);
    auto const nested = input_domain_t{0.0, 3.0};
    auto const domain = make_domain_sut(nested).input_domain();
    auto const successor = std::nextafter(domain.last(), std::numeric_limits<scalar_t>::infinity());

    EXPECT_TRUE(nested.contains(warp.apply(domain.last())));
    EXPECT_FALSE(nested.contains(warp.apply(successor)));
}

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
