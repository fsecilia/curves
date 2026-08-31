// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/jet/jet.hpp>
#include <cassert>
#include <cmath>
#include <concepts>
#include <expected>
#include <limits>
#include <optional>

namespace crv::shaping::transforms {

enum class output_scale_error_t : uint8_t
{
    scale_not_finite,
    scale_not_positive,
};

/// positive output scale
///
/// Evaluates S(y) = scale * y.
template <std::floating_point t_scalar_t> class output_scale_t
{
public:
    using scalar_t = t_scalar_t;
    using jet_t = crv::jet_t<scalar_t>;
    using construction_result_t = std::expected<output_scale_t, output_scale_error_t>;

    [[nodiscard]] static auto make(scalar_t scale) -> construction_result_t
    {
        if (!std::isfinite(scale)) return std::unexpected{output_scale_error_t::scale_not_finite};
        if (scale <= scalar_t{0}) return std::unexpected{output_scale_error_t::scale_not_positive};
        return output_scale_t{scale};
    }

    [[nodiscard]] auto try_apply(scalar_t output) const noexcept -> std::optional<scalar_t>
    {
        if (!is_representable(output)) return std::nullopt;
        return scale_ * output;
    }

    [[nodiscard]] auto apply(scalar_t output) const noexcept -> scalar_t
    {
        assert(is_representable(output) && "output_scale_t: scaled output must be representable");
        return scale_ * output;
    }

    [[nodiscard]] auto apply(jet_t output) const noexcept -> jet_t
    {
        assert(is_representable(primal(output)) && "output_scale_t: scaled jet primal must be representable");
        return scale_ * output;
    }

private:
    [[nodiscard]] auto is_representable(scalar_t output) const noexcept -> bool
    {
        if (!std::isfinite(output)) return false;

        auto constexpr lowest = std::numeric_limits<scalar_t>::lowest();
        auto constexpr max = std::numeric_limits<scalar_t>::max();
        if (output > scalar_t{0} && output > max / scale_) return false;
        if (output < scalar_t{0} && output < lowest / scale_) return false;
        return true;
    }

    constexpr explicit output_scale_t(scalar_t scale) noexcept : scale_{scale} {}

    scalar_t scale_;
};

} // namespace crv::shaping::transforms
