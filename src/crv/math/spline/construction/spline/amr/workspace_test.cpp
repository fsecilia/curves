// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "workspace.hpp"
#include <crv/test/test.hpp>
#include <functional>

namespace crv::spline::generic {
namespace {

struct workspace_test_t : Test
{
    struct interval_t
    {
        struct subdomain_t
        {
            constexpr auto operator<=>(subdomain_t const&) const noexcept -> auto = default;
        };

        constexpr auto operator<=>(interval_t const&) const noexcept -> auto = default;
    };
    static constexpr auto max_segments = 256;

    using sut_t = workspace_t<interval_t, std::less<>, max_segments>;
};

TEST_F(workspace_test_t, clear_forwards_to_members)
{
    auto sut = sut_t{};

    sut.completed_intervals.push_back({});
    sut.refinement_pool.push({});

    EXPECT_FALSE(sut.completed_intervals.empty());
    EXPECT_FALSE(sut.refinement_pool.empty());

    sut.clear();

    EXPECT_TRUE(sut.completed_intervals.empty());
    EXPECT_TRUE(sut.refinement_pool.empty());
}

} // namespace
} // namespace crv::spline::generic
