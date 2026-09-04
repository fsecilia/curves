// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "output_position.hpp"
#include <crv/test/test.hpp>
#include <limits>
#include <utility>

namespace crv::shaping::transforms {
namespace {

using scalar_t = float_t;
using jet_t = crv::jet_t<scalar_t>;
using sut_t = output_position_t<scalar_t>;

[[nodiscard]] auto construct_sut(scalar_t source_level, scalar_t target_level) -> sut_t
{
    return std::move(sut_t::construct(source_level, target_level)).value();
}

TEST(shaping_transforms_output_position_test_t, translates_source_level_to_target_level)
{
    EXPECT_EQ(construct_sut(3.0, 7.0).apply(3.0), 7.0);
}

TEST(shaping_transforms_output_position_test_t, translates_relative_output)
{
    EXPECT_EQ(construct_sut(0.0, -2.0).apply(5.0), 3.0);
}

TEST(shaping_transforms_output_position_test_t, preserves_small_fixed_target_exactly)
{
    auto const target = scalar_t{1e-12};
    EXPECT_EQ(construct_sut(999000.0, target).apply(999000.0), target);
}

TEST(shaping_transforms_output_position_test_t, leaves_jet_tangent_unchanged)
{
    EXPECT_EQ(construct_sut(3.0, 7.0).apply(jet_t{5.0, 11.0}), (jet_t{9.0, 11.0}));
}

TEST(shaping_transforms_output_position_test_t, rejects_nonfinite_source_level)
{
    EXPECT_EQ(sut_t::construct(std::numeric_limits<scalar_t>::infinity(), 0.0),
        std::unexpected{output_position_error_t::source_level_not_finite});
}

TEST(shaping_transforms_output_position_test_t, rejects_nonfinite_target_level)
{
    EXPECT_EQ(sut_t::construct(0.0, std::numeric_limits<scalar_t>::quiet_NaN()),
        std::unexpected{output_position_error_t::target_level_not_finite});
}

TEST(shaping_transforms_output_position_test_t, rejects_overflowing_subtraction_even_when_later_addition_could_cancel)
{
    auto const max = std::numeric_limits<scalar_t>::max();
    EXPECT_FALSE(construct_sut(-max, -max).try_apply(max));
}

TEST(shaping_transforms_output_position_test_t, rejects_overflowing_final_addition)
{
    auto const max = std::numeric_limits<scalar_t>::max();
    EXPECT_FALSE(construct_sut(0.0, max).try_apply(max));
}

} // namespace
} // namespace crv::shaping::transforms
