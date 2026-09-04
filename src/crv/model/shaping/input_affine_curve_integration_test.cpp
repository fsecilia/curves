// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "input_affine_curve.hpp"
#include <crv/model/curves/power_law.hpp>
#include <crv/model/domain.hpp>
#include <crv/model/shaping/transforms/input_affine.hpp>
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
using transform_t = transforms::input_affine_t<scalar_t>;

struct identity_curve_t
{
    using scalar_t = crv::float_t;

    input_domain_t domain{input_domain_t::full()};
    std::vector<scalar_t> points{};

    [[nodiscard]] constexpr auto operator()(scalar_t input) const noexcept -> scalar_t { return input; }
    [[nodiscard]] constexpr auto operator()(jet_t input) const noexcept -> jet_t { return input; }
    [[nodiscard]] constexpr auto input_domain() const noexcept -> input_domain_t { return domain; }
    [[nodiscard]] auto critical_points() const -> std::vector<scalar_t> { return points; }
};

using sut_t = input_affine_curve_t<transform_t, identity_curve_t>;

[[nodiscard]] auto construct_sut(scalar_t scale, scalar_t shift, input_domain_t domain = input_domain_t::full(),
    std::vector<scalar_t> points = {}) -> sut_t
{
    return {std::move(transform_t::construct(scale, shift)).value(), identity_curve_t{domain, std::move(points)}};
}

TEST(input_affine_curve_integration_test_t, composes_real_transform_for_scalar)
{
    EXPECT_EQ(construct_sut(3.0, 2.0)(5.0), 9.0);
}

TEST(input_affine_curve_integration_test_t, composes_real_transform_for_jet)
{
    EXPECT_EQ(construct_sut(2.0, 3.0)(jet_t{5.0, 7.0}), (jet_t{4.0, 14.0}));
}

TEST(input_affine_domain_integration_test_t, maps_ordinary_interval_exactly)
{
    auto const sut = construct_sut(2.0, 1.0, {-4.0, 10.0});
    auto const first = std::nextafter(-1.0, std::numeric_limits<scalar_t>::lowest());
    EXPECT_EQ(sut.input_domain(), (input_domain_t{first, 6.0}));
}

TEST(input_affine_domain_integration_test_t, resolves_expanding_scale)
{
    auto const sut = construct_sut(4.0, 0.0, {-8.0, 12.0});
    EXPECT_EQ(sut.input_domain(), (input_domain_t{-2.0, 3.0}));
}

TEST(input_affine_domain_integration_test_t, resolves_contracting_scale)
{
    auto const sut = construct_sut(0.5, 0.0, {-2.0, 3.0});
    EXPECT_EQ(sut.input_domain(), (input_domain_t{-4.0, 6.0}));
}

TEST(input_affine_domain_integration_test_t, supports_negative_outer_interval)
{
    auto const transform = transform_t::construct(2.0, 1.0).value();
    auto const nested = input_domain_t{-4.0, -3.0};
    auto const domain = sut_t{transform, identity_curve_t{nested}}.input_domain();

    EXPECT_LT(domain.last(), 0.0);
    EXPECT_TRUE(nested.contains(transform.apply(domain.first())));
    EXPECT_TRUE(nested.contains(transform.apply(domain.last())));
    EXPECT_FALSE(
        nested.contains(transform.apply(std::nextafter(domain.first(), std::numeric_limits<scalar_t>::lowest()))));
    EXPECT_FALSE(nested.contains(transform.apply(std::nextafter(domain.last(), std::numeric_limits<scalar_t>::max()))));
}

TEST(input_affine_domain_integration_test_t, preserves_empty_nested_domain)
{
    EXPECT_TRUE(construct_sut(2.0, 1.0, {}).input_domain().empty());
}

TEST(input_affine_domain_integration_test_t, forward_representability_limits_outer_interval)
{
    auto const transform = transform_t::construct(2.0, 0.0).value();
    auto const sut = sut_t{transform, identity_curve_t{}};
    auto const domain = sut.input_domain();
    auto const lowest = std::numeric_limits<scalar_t>::lowest();
    auto const max = std::numeric_limits<scalar_t>::max();

    EXPECT_TRUE(transform.try_apply(domain.first()).has_value());
    EXPECT_TRUE(transform.try_apply(domain.last()).has_value());
    EXPECT_FALSE(transform.try_apply(std::nextafter(domain.first(), lowest)).has_value());
    EXPECT_FALSE(transform.try_apply(std::nextafter(domain.last(), max)).has_value());
}

TEST(input_affine_domain_integration_test_t, corrects_inverse_seed_that_rounds_one_ulp_outside_nested_upper)
{
    auto const transform = transform_t::construct(0.1, 0.0).value();
    auto const sut = sut_t{transform, identity_curve_t{{0.0, 1.7}}};
    auto const domain = sut.input_domain();

    EXPECT_LT(domain.last(), 17.0);
    EXPECT_LE(transform.apply(domain.last()), 1.7);
    EXPECT_GT(transform.apply(std::nextafter(domain.last(), std::numeric_limits<scalar_t>::infinity())), 1.7);
}

TEST(input_affine_domain_integration_test_t, returned_first_is_exact_forward_boundary)
{
    auto const transform = transform_t::construct(0.1, 0.0).value();
    auto const nested = input_domain_t{1.7, 3.0};
    auto const domain = sut_t{transform, identity_curve_t{nested}}.input_domain();
    auto const predecessor = std::nextafter(domain.first(), -std::numeric_limits<scalar_t>::infinity());

    EXPECT_TRUE(nested.contains(transform.apply(domain.first())));
    auto const predecessor_output = transform.try_apply(predecessor);
    EXPECT_TRUE(!predecessor_output || !nested.contains(*predecessor_output));
}

TEST(input_affine_domain_integration_test_t, returned_last_is_exact_forward_boundary)
{
    auto const transform = transform_t::construct(0.1, 0.0).value();
    auto const nested = input_domain_t{-3.0, 1.7};
    auto const domain = sut_t{transform, identity_curve_t{nested}}.input_domain();
    auto const successor = std::nextafter(domain.last(), std::numeric_limits<scalar_t>::infinity());

    EXPECT_TRUE(nested.contains(transform.apply(domain.last())));
    auto const successor_output = transform.try_apply(successor);
    EXPECT_TRUE(!successor_output || !nested.contains(*successor_output));
}

TEST(input_affine_curve_integration_test_t, inverse_maps_multiple_critical_points)
{
    auto const sut = construct_sut(2.0, 1.0, input_domain_t::full(), {2.0, 6.0});
    EXPECT_EQ(sut.critical_points(), (std::vector<scalar_t>{2.0, 4.0}));
}

TEST(input_affine_power_law_integration_test_t, propagates_power_law_input_domain_not_output_frontier)
{
    using power_law_t = model::curves::power_law_t;
    using evaluator_t = power_law_t::evaluator_t<scalar_t>;
    using curve_t = input_affine_curve_t<transform_t, evaluator_t>;

    auto const raw = evaluator_t{power_law_t::params_t<scalar_t>{1.0, 256.0}};
    auto const transform = transform_t::construct(2.0, 1.0).value();
    auto const sut = curve_t{transform, raw};

    EXPECT_TRUE(sut.input_domain().contains(9.0));
    EXPECT_TRUE(sut.input_domain().contains(std::nextafter(9.0, std::numeric_limits<scalar_t>::infinity())));
}

TEST(input_affine_power_law_integration_test_t, preserves_power_law_left_scalar_boundary)
{
    using power_law_t = model::curves::power_law_t;
    using evaluator_t = power_law_t::evaluator_t<scalar_t>;
    using curve_t = input_affine_curve_t<transform_t, evaluator_t>;

    auto const raw = evaluator_t{power_law_t::params_t<scalar_t>{1.0, 0.5}};
    auto const transform = transform_t::construct(2.0, 1.0).value();
    auto const sut = curve_t{transform, raw};

    EXPECT_EQ(sut.input_domain().first(), 1.0);
}

} // namespace
} // namespace crv::shaping
