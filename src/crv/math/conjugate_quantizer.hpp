// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <cmath>
#include <utility>

namespace crv {

/// quantizes projected values in an unprojected space via algebraic conjugation
///
/// This type unprojects values by inverting a projection, quantizes in that space, then projects back to the original
/// space:
///
///    q = f(Q(f^-1(p))
///    quantized = project(quantize(unproject(p, projection)))
///
/// This specific implementation uses a strictly-positive nearest-neighbor. It will never quantize to 0.
template <typename real_t, real_t quantum, typename projection_t, typename inverter_t> class conjugate_quantizer_t
{
public:
    static_assert(quantum > real_t{0});

    constexpr conjugate_quantizer_t(
        real_t unprojected_min, real_t unprojected_max, projection_t projection, inverter_t invert = {}) noexcept
        : unprojected_min_{unprojected_min}, unprojected_max_{unprojected_max}, projection_{std::move(projection)},
          invert_{std::move(invert)}
    {}

    [[nodiscard]] constexpr auto operator()(real_t projected) const -> real_t
    {
        // unproject via inverse
        auto const unprojected
            = invert_(unprojected_min_, unprojected_max_, projected, projection_).value_or(unprojected_max_);

        // quantize using strictly-positive nearest-neighbor
        auto const quantized = std::max(quantum, std::round(unprojected / quantum) * quantum);

        // reproject
        return projection_(quantized);
    }

private:
    real_t unprojected_min_;
    real_t unprojected_max_;
    projection_t projection_;
    [[no_unique_address]] inverter_t invert_;
};

} // namespace crv
