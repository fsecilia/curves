// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "node_generator.hpp"
#include <crv/test/test.hpp>

namespace crv::spline::node_generators {
namespace {

struct node_generators_equioscillation_test_t : Test
{
    auto compare(auto const& expected, auto const& actual) const noexcept -> void
    {
        constexpr auto count = std::size(expected);
        static_assert(count == std::size(actual));

        for (auto sample = 0u; sample < count; ++sample) { EXPECT_DOUBLE_EQ(expected[sample], actual[sample]); }
    }
};

TEST_F(node_generators_equioscillation_test_t, count_3)
{
    // clang-format off
    static auto const expected = std::array{
        1.4644660940672621e-01,
        5.0000000000000000e-01,
        8.5355339059327373e-01,
    };
    // clang-format on

    compare(expected, equioscillation_t<float_t, 3>{}());
}

TEST_F(node_generators_equioscillation_test_t, count_5)
{
    // clang-format off
    static auto const expected = std::array{
        6.6987298107780646e-02,
        2.4999999999999994e-01,
        5.0000000000000000e-01,
        7.4999999999999989e-01,
        9.3301270189221919e-01,
    };
    // clang-format on

    compare(expected, equioscillation_t<float_t, 5>{}());
}

} // namespace
} // namespace crv::spline::node_generators
