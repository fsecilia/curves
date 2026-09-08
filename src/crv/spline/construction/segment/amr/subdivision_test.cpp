// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "subdivision.hpp"
#include <crv/math/fixed/fixed.hpp>
#include <crv/spline/construction/error.hpp>
#include <crv/test/test.hpp>
#include <expected>
#include <optional>
#include <vector>

namespace crv::spline {
namespace {

struct spline_subdivider_test_t : Test
{
    using scalar_t = float_t;
    using x_t = fixed_t<int_t, 0>;
    using error_t = spline_construction_error_t<x_t>;

    static constexpr auto sampler_id = int_t{3};
    static constexpr auto sample_target = [] { return sampler_id; };

    struct subdomain_t
    {
        int_t id;
        constexpr auto operator==(subdomain_t const&) const noexcept -> bool = default;
    };

    struct interval_t
    {
        using scalar_t = spline_subdivider_test_t::scalar_t;

        subdomain_t subdomain;
        int_t factory_id;
        constexpr auto operator==(interval_t const&) const noexcept -> bool = default;
    };

    using subdivision_t = spline::subdivision_t<interval_t>;

    struct bisection_t
    {
        subdomain_t left;
        subdomain_t right;
    };

    struct bisector_t
    {
        constexpr auto operator()(auto const& target, subdomain_t const& parent) const noexcept -> bisection_t
        {
            return {.left = {parent.id + 10 + target()}, .right = {parent.id + 20}};
        }
    };

    struct interval_factory_t
    {
        using interval_t = spline_subdivider_test_t::interval_t;
        using error_t = spline_subdivider_test_t::error_t;

        std::optional<int_t> failing_subdomain_id;
        std::vector<int_t>* constructed_ids = nullptr;

        auto operator()(auto const& target, subdomain_t const& subdomain) const noexcept
            -> std::expected<interval_t, error_t>
        {
            constructed_ids->push_back(subdomain.id);
            if (failing_subdomain_id == subdomain.id)
            {
                return std::unexpected{error_t{
                    .reason = spline_construction_error_reason_t::gain_anchor_not_representable,
                    .left = x_t{subdomain.id},
                    .right = x_t{subdomain.id + 1},
                }};
            }
            return interval_t{.subdomain = subdomain, .factory_id = 99 + target()};
        }
    };

    std::vector<int_t> constructed_ids;
    using sut_t = subdivider_t<subdivision_t, bisector_t, interval_factory_t>;
    sut_t sut{
        .bisect = {}, .create_interval = {.failing_subdomain_id = std::nullopt, .constructed_ids = &constructed_ids}};
    interval_t const parent{.subdomain = {.id = 5}, .factory_id = 0};
};

TEST_F(spline_subdivider_test_t, constructs_both_children)
{
    auto const result = sut(sample_target, parent);

    ASSERT_TRUE(result);
    EXPECT_EQ(result->left, (interval_t{.subdomain = {.id = 18}, .factory_id = 102}));
    EXPECT_EQ(result->right, (interval_t{.subdomain = {.id = 25}, .factory_id = 102}));
    EXPECT_EQ(constructed_ids, (std::vector<int_t>{18, 25}));
}

TEST_F(spline_subdivider_test_t, left_child_failure_is_propagated_without_constructing_right_child)
{
    sut.create_interval.failing_subdomain_id = 18;

    auto const result = sut(sample_target, parent);

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().left, x_t{18});
    EXPECT_EQ(constructed_ids, (std::vector<int_t>{18}));
}

TEST_F(spline_subdivider_test_t, right_child_failure_discards_successful_left_child)
{
    sut.create_interval.failing_subdomain_id = 25;

    auto const result = sut(sample_target, parent);

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().left, x_t{25});
    EXPECT_EQ(constructed_ids, (std::vector<int_t>{18, 25}));
}

} // namespace
} // namespace crv::spline
