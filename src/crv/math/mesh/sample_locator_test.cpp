// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "sample_locator.hpp"
#include <crv/test/test.hpp>

namespace crv::equioscillation {
namespace {

// --------------------------------------------------------------------------------------------------------------------
// Node Cache
// --------------------------------------------------------------------------------------------------------------------

struct equioscillation_cached_nodes_test_t : Test
{
    auto compare(auto const& expected, auto const& actual) const noexcept -> void
    {
        constexpr auto count = std::size(expected);
        static_assert(count == std::size(actual));

        for (auto node = 0u; node < count; ++node) { EXPECT_DOUBLE_EQ(expected[node], actual[node]); }
    }
};

TEST_F(equioscillation_cached_nodes_test_t, count_5)
{
    // clang-format off
    static auto const expected = std::array{
        -1.000000000000000e+00,
        -7.071067811865475e-01,
         0.000000000000000e+00,
         7.071067811865474e-01,
         1.000000000000000e+00,
    };
    // clang-format on

    compare(expected, node_cache_t<float_t, 5>::nodes);
}

TEST_F(equioscillation_cached_nodes_test_t, count_7)
{
    // clang-format off
    static auto const expected = std::array{
        -1.000000000000000e+00,
        -8.660254037844387e-01,
        -5.000000000000001e-01,
         0.000000000000000e+00,
         4.999999999999997e-01,
         8.660254037844384e-01,
         1.000000000000000e+00,
    };
    // clang-format on

    compare(expected, node_cache_t<float_t, 7>::nodes);
}

} // namespace
} // namespace crv::equioscillation
