// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/curves/concepts.hpp>
#include <crv/math/scalar_traits.hpp>
#include <cassert>
#include <concepts>
#include <optional>

namespace crv::shaping::construction {

/// matches operator ()(real_t, real_t, real_t, curve_t) const noexcept -> std::optional<real_t>
template <typename inverter_t, typename real_t, typename curve_t>
concept is_inverter
    = std::floating_point<real_t> && std::is_nothrow_invocable_v<inverter_t, real_t, real_t, real_t, curve_t const&>
    && requires(inverter_t const& inverter, real_t min, real_t max, real_t limit, curve_t const& curve) {
           { inverter(min, max, limit, curve) } -> std::convertible_to<std::optional<real_t>>;
       };

/// locates limit transition
template <std::floating_point real_t, typename inverter_t> struct limit_locator_t
{
    inverter_t invert;

    /// locates curve input where transition must begin for it to smoothly hit output limit over transition_width
    ///
    /// \param input_min domain min to invert against
    /// \param input_max domain max to invert against
    /// \param transition_width width of transition in domain
    /// \param transition_max max value of normalized transition; transition(1)
    /// \param output_limit target limit in function output
    /// \pre curve is monotonic non-decreasing on [input_min, input_max]
    /// \returns input to begin transition
    template <is_curve<real_t> curve_t>
        requires is_inverter<inverter_t, real_t, curve_t>
    constexpr auto operator()(real_t input_min, real_t input_max, real_t transition_width, real_t transition_max,
        real_t output_limit, curve_t const& curve) const noexcept -> real_t
    {
        auto const input_limit_opt = invert(input_min, input_max, output_limit, curve);
        auto const input_limited = input_limit_opt.value_or(input_max);
        auto const limit_start = input_limited - transition_width * transition_max;

        return limit_start;
    }
};

} // namespace crv::shaping::construction
