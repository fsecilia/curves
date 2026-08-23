// SPDX-License-Identifier: MIT

/// \file
/// \brief spline construction result
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <optional>

namespace crv::spline {

enum class spline_generation_error_reason_t
{
    segment_budget_exhausted,
    minimum_interval_width,
};

template <is_fixed t_x_t> struct spline_generation_error_t
{
    using x_t = t_x_t;

    spline_generation_error_reason_t reason;
    x_t left;
    x_t right;

    constexpr auto operator==(spline_generation_error_t const&) const noexcept -> bool = default;
};

template <is_fixed t_x_t> struct spline_generation_result_t
{
    using x_t = t_x_t;
    using error_t = spline_generation_error_t<x_t>;

    std::optional<error_t> error;

    constexpr explicit operator bool() const noexcept { return !error.has_value(); }
    constexpr auto operator==(spline_generation_result_t const&) const noexcept -> bool = default;
};

} // namespace crv::spline
