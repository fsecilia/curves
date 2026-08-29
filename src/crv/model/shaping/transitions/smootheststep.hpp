// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/jet/jet.hpp>
#include <concepts>

namespace crv::shaping::transitions {

/// compact C3 smootheststep transition
struct smootheststep_t
{
    template <std::floating_point scalar_t>
    [[nodiscard]] constexpr auto operator()(scalar_t u) const noexcept -> scalar_t
    {
        if (u <= scalar_t{0}) return scalar_t{0};
        if (u >= scalar_t{1}) return scalar_t{1};
        auto const u2 = u * u;
        auto const u4 = u2 * u2;
        return u4 * (((-scalar_t{20} * u + scalar_t{70}) * u - scalar_t{84}) * u + scalar_t{35});
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
        auto const complement = scalar_t{1} - u;
        return scalar_t{140} * u * u * u * complement * complement * complement;
    }

    template <std::floating_point scalar_t>
    [[nodiscard]] constexpr auto antiderivative(scalar_t u) const noexcept -> scalar_t
    {
        if (u <= scalar_t{0}) return scalar_t{0};
        if (u >= scalar_t{1}) return u - scalar_t{0.5};
        auto const u2 = u * u;
        auto const u4 = u2 * u2;
        auto const u5 = u4 * u;
        return u5 * (((-scalar_t{2.5} * u + scalar_t{10}) * u - scalar_t{14}) * u + scalar_t{7});
    }

    template <std::floating_point scalar_t>
    [[nodiscard]] constexpr auto antiderivative(jet_t<scalar_t> u) const noexcept -> jet_t<scalar_t>
    {
        auto const value = primal(u);
        return {antiderivative(value), operator()(value) * tangent(u)};
    }
};

} // namespace crv::shaping::transitions
