// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/jet/jet.hpp>
#include <concepts>

namespace crv::shaping::transitions {

/// compact C1 smoothstep transition
struct smoothstep_t
{
    template <std::floating_point scalar_t>
    [[nodiscard]] constexpr auto operator()(scalar_t u) const noexcept -> scalar_t
    {
        if (u <= scalar_t{0}) return scalar_t{0};
        if (u >= scalar_t{1}) return scalar_t{1};
        return u * u * (scalar_t{3} - scalar_t{2} * u);
    }

    template <std::floating_point scalar_t>
    [[nodiscard]] constexpr auto operator()(jet_t<scalar_t> u) const noexcept -> jet_t<scalar_t>
    {
        auto const value = primal(u);
        return {operator()(value), derivative(value) * tangent(u)};
    }

    template <std::floating_point scalar_t>
    [[nodiscard]] constexpr auto derivative(scalar_t u) const noexcept -> scalar_t
    {
        if (u <= scalar_t{0} || u >= scalar_t{1}) return scalar_t{0};
        return scalar_t{6} * u * (scalar_t{1} - u);
    }

    template <std::floating_point scalar_t>
    [[nodiscard]] constexpr auto antiderivative(scalar_t u) const noexcept -> scalar_t
    {
        if (u <= scalar_t{0}) return scalar_t{0};
        if (u >= scalar_t{1}) return u - scalar_t{0.5};
        auto const u2 = u * u;
        return u2 * u * (scalar_t{1} - u / scalar_t{2});
    }

    template <std::floating_point scalar_t>
    [[nodiscard]] constexpr auto antiderivative(jet_t<scalar_t> u) const noexcept -> jet_t<scalar_t>
    {
        auto const value = primal(u);
        return {antiderivative(value), operator()(value) * tangent(u)};
    }
};

} // namespace crv::shaping::transitions
