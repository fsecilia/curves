// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/jet/jet.hpp>
#include <concepts>

namespace crv::shaping::transitions {

/// compact C2 smootherstep transition
struct smootherstep_t
{
    template <std::floating_point scalar_t> [[nodiscard]] constexpr auto value(scalar_t u) const noexcept -> scalar_t
    {
        if (u <= scalar_t{0}) return scalar_t{0};
        if (u >= scalar_t{1}) return scalar_t{1};
        auto const u2 = u * u;
        auto const u3 = u2 * u;
        return u3 * ((scalar_t{6} * u - scalar_t{15}) * u + scalar_t{10});
    }

    template <std::floating_point scalar_t>
    [[nodiscard]] constexpr auto value(jet_t<scalar_t> u) const noexcept -> jet_t<scalar_t>
    {
        auto const value = primal(u);
        return {this->value(value), derivative(value) * tangent(u)};
    }

    template <std::floating_point scalar_t>
    [[nodiscard]] constexpr auto derivative(scalar_t u) const noexcept -> scalar_t
    {
        if (u <= scalar_t{0} || u >= scalar_t{1}) return scalar_t{0};
        auto const complement = scalar_t{1} - u;
        return scalar_t{30} * u * u * complement * complement;
    }

    template <std::floating_point scalar_t>
    [[nodiscard]] constexpr auto antiderivative(scalar_t u) const noexcept -> scalar_t
    {
        if (u <= scalar_t{0}) return scalar_t{0};
        if (u >= scalar_t{1}) return u - scalar_t{0.5};
        auto const u2 = u * u;
        auto const u4 = u2 * u2;
        return u4 * ((u - scalar_t{3}) * u + scalar_t{2.5});
    }

    template <std::floating_point scalar_t>
    [[nodiscard]] constexpr auto antiderivative(jet_t<scalar_t> u) const noexcept -> jet_t<scalar_t>
    {
        auto const value = primal(u);
        return {antiderivative(value), this->value(value) * tangent(u)};
    }
};

} // namespace crv::shaping::transitions
