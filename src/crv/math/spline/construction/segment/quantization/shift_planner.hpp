// SPDX-License-Identifier: MIT

/// \file
/// \brief determines dynamic shifts during evaluation
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <algorithm>

namespace crv::spline {

/// determines safe relative shifts to align radices between products and the next coefficient during evaluation
///
/// The evaluator only shifts right. This type determines how much to shift between types at runtime when a right shift
/// is sufficient. When the loop would require a left shift, it also determines how much to destructively shift the next
/// coefficient right during construction to bring it into range.
struct shift_planner_t
{
    struct plan_t
    {
        int_t packed_runtime_shift; ///< relative shift performed at runtime during evaluation
        int_t destructive_preshift; ///< shift applied to next coefficient before packing
        int_t next_accumulator_exponent; ///< state carried to next planning step

        auto operator==(plan_t const&) const noexcept -> bool = default;
    };

    constexpr auto operator()(int_t accumulator_exponent, int_t next_exponent, int_t t_to_x_shift) const noexcept
        -> plan_t
    {
        auto const relative_shift = next_exponent - accumulator_exponent;

        return {
            .packed_runtime_shift = t_to_x_shift + std::max<int_t>(0, relative_shift),
            .destructive_preshift = std::max<int_t>(0, -relative_shift),
            .next_accumulator_exponent = std::max(accumulator_exponent, next_exponent),
        };
    }
};

} // namespace crv::spline
