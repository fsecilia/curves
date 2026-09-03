// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "composed_curve.hpp"
#include <crv/test/test.hpp>
#include <variant>

namespace crv::model::curves {
namespace {

using scalar_t = float_t;

template <typename curve_t> using evaluator_t = curve_t::template evaluator_t<scalar_t>;
template <typename curve_t> using evaluated_curve_t = shaping::curve_evaluator_t<evaluator_t<curve_t>>;

struct composed_curve_test_t : Test
{};

TEST_F(composed_curve_test_t, synchronous_construction_retains_curve_evaluator)
{
    auto const curve = create_composed_curve<scalar_t>(synchronous_t::config_t{});
    EXPECT_TRUE(std::holds_alternative<evaluated_curve_t<synchronous_t>>(curve.variant));
}

TEST_F(composed_curve_test_t, log_normal_construction_retains_curve_evaluator)
{
    auto const curve = create_composed_curve<scalar_t>(log_normal_t::config_t{});
    EXPECT_TRUE(std::holds_alternative<evaluated_curve_t<log_normal_t>>(curve.variant));
}

TEST_F(composed_curve_test_t, smooth_gain_construction_retains_curve_evaluator)
{
    auto const curve = create_composed_curve<scalar_t>(smooth_gain_t::config_t{});
    EXPECT_TRUE(std::holds_alternative<evaluated_curve_t<smooth_gain_t>>(curve.variant));
}

} // namespace
} // namespace crv::model::curves
