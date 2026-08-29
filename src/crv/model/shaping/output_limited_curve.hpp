// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/jet/jet.hpp>
#include <crv/model/shaping/transforms/compact_output_limiter.hpp>
#include <utility>

namespace crv::shaping {

/// composes an output limiter over a curve while preserving exact plateau control flow
template <typename t_limiter_t, typename t_curve_t> class output_limited_curve_t
{
public:
    using limiter_t = t_limiter_t;
    using curve_t = t_curve_t;
    using scalar_t = typename limiter_t::scalar_t;
    using jet_t = crv::jet_t<scalar_t>;

    constexpr output_limited_curve_t(limiter_t limiter, curve_t curve) noexcept
        : limiter_{std::move(limiter)}, curve_{std::move(curve)}
    {}

    [[nodiscard]] auto operator()(scalar_t input) const noexcept -> scalar_t
    {
        if (limiter_.is_globally_constant()) return limiter_.bound();
        if (limiter_.is_globally_identity()) return curve_(input);
        return limiter_(curve_(input));
    }

    [[nodiscard]] auto operator()(jet_t input) const noexcept -> jet_t
    {
        if (limiter_.is_globally_constant()) return jet_t{limiter_.bound()};
        if (limiter_.is_globally_identity()) return curve_(input);

        auto const curve_value = curve_(primal(input));
        if (limiter_.classify(curve_value) == transforms::output_limiter_region_t::plateau)
        {
            return jet_t{limiter_.bound()};
        }

        return limiter_(curve_(input));
    }

    [[nodiscard]] constexpr auto limiter() const noexcept -> limiter_t const& { return limiter_; }
    [[nodiscard]] constexpr auto curve() const noexcept -> curve_t const& { return curve_; }

private:
    limiter_t limiter_;
    curve_t curve_;
};

} // namespace crv::shaping
