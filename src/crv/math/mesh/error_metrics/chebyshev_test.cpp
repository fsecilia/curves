// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include <crv/math/mesh/error_metrics/chebyshev.hpp>
#include <crv/test/test.hpp>

namespace crv::chebyshev {
namespace {

// --------------------------------------------------------------------------------------------------------------------
// Node Cache
// --------------------------------------------------------------------------------------------------------------------

struct chebyshev_cached_nodes_test_t : Test
{
    auto compare(auto const& expected, auto const& actual) const noexcept -> void
    {
        constexpr auto count = std::size(expected);
        static_assert(count == std::size(actual));

        for (auto node = 0u; node < count; ++node) { EXPECT_NEAR(expected[node], actual[node], 1e-10); }
    }
};

TEST_F(chebyshev_cached_nodes_test_t, count_5)
{
    static auto const expected = std::array{
        9.510565162951535311819384332921e-01,  5.877852522924731371034567928291e-01,
        6.123233995736766035868820147292e-17,  -5.877852522924730260811543303134e-01,
        -9.510565162951535311819384332921e-01,
    };

    compare(expected, node_cache_t<float_t, 5>::nodes);
}

TEST_F(chebyshev_cached_nodes_test_t, count_7)
{
    static auto const expected = std::array{
        9.749279121818236193419693336182e-01,  7.818314824680298036341241640912e-01,
        4.338837391175581759128476733167e-01,  6.123233995736766035868820147292e-17,
        -4.338837391175580648905452108011e-01, -7.818314824680298036341241640912e-01,
        -9.749279121818236193419693336182e-01,
    };

    compare(expected, node_cache_t<float_t, 7>::nodes);
}

} // namespace
} // namespace crv::chebyshev
