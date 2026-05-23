// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/int_traits.hpp>
#include <crv/math/integer.hpp>
#include <algorithm>
#include <vector>

namespace crv::spline::seed {

/// quantizes, sorts, and uniques set of critical points
template <is_fixed x_t, int_t log2_min_width> struct critical_point_conditioner_t
{
    using points_t = std::vector<x_t>;

    /// quantizes point to min segment width grid
    constexpr auto operator()(x_t const& point) const -> x_t { return quantize(point); }

    /// quantizes eacn point to min segment width grid, then sorts and uniques result
    constexpr auto operator()(points_t points) const -> points_t
    {
        for (auto& point : points) point = quantize(point);

        // sort and unique
        std::ranges::sort(points);
        auto const [first_dup, last] = std::ranges::unique(points);
        points.erase(first_dup, last);

        return points;
    }

private:
    constexpr auto quantize(x_t const& point) const noexcept -> x_t
    {
        constexpr auto align_shift = int_cast<int_t>(x_t::frac_bits + log2_min_width);
        static_assert(align_shift > 0);

        using unsigned_t = make_unsigned_t<typename x_t::value_t>;
        constexpr auto align_mask = (unsigned_t{1} << align_shift) - 1;
        constexpr auto align_bias = unsigned_t{1} << (align_shift - 1);

        // crack fixed
        auto val = static_cast<unsigned_t>(point.value) + align_bias;

        // apply bitwise alignment to underlying
        val &= ~align_mask;

        // repack fixed
        return x_t::literal(static_cast<typename x_t::value_t>(val));
    }
};

} // namespace crv::spline::seed
