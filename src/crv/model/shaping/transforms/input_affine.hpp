// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/inverse.hpp>
#include <crv/math/jet/jet.hpp>
#include <crv/model/domain.hpp>
#include <cassert>
#include <cmath>
#include <concepts>
#include <expected>
#include <limits>
#include <optional>

namespace crv::shaping::transforms {

enum class input_affine_error_t : uint8_t
{
    scale_not_finite,
    scale_not_positive,
    shift_not_finite,
};

/// direct positive-scale affine input transform
///
/// Evaluates A(x) = scale * (x - shift) with arithmetic ordered to avoid intermediate overflow.
template <std::floating_point t_scalar_t> class input_affine_t
{
public:
    using scalar_t = t_scalar_t;
    using jet_t = crv::jet_t<scalar_t>;
    using construction_result_t = std::expected<input_affine_t, input_affine_error_t>;

    [[nodiscard]] static auto construct(scalar_t scale, scalar_t shift) -> construction_result_t
    {
        if (!std::isfinite(scale)) return std::unexpected{input_affine_error_t::scale_not_finite};
        if (scale <= scalar_t{0}) return std::unexpected{input_affine_error_t::scale_not_positive};
        if (!std::isfinite(shift)) return std::unexpected{input_affine_error_t::shift_not_finite};
        return input_affine_t{scale, shift};
    }

    [[nodiscard]] auto try_apply(scalar_t input) const noexcept -> std::optional<scalar_t>
    {
        if (!std::isfinite(input)) return std::nullopt;

        auto const output = scale_ >= scalar_t{1} ? scale_ * (input - shift_) : scale_ * input - scale_ * shift_;
        if (!std::isfinite(output)) return std::nullopt;
        return output;
    }

    [[nodiscard]] auto apply(scalar_t input) const noexcept -> scalar_t
    {
        auto const output = try_apply(input);
        assert(output.has_value() && "input_affine_t: transformed scalar must be representable");
        return *output;
    }

    [[nodiscard]] auto apply(jet_t input) const noexcept -> jet_t
    {
        assert(try_apply(primal(input)).has_value() && "input_affine_t: transformed jet primal must be representable");
        if (scale_ >= scalar_t{1}) return scale_ * (input - shift_);
        return scale_ * input - scale_ * shift_;
    }

    /// exact outer-input preimage of a nested input domain
    [[nodiscard]] auto preimage(model::input_domain_t<scalar_t> nested_domain) const noexcept
        -> model::input_domain_t<scalar_t>
    {
        using domain_t = model::input_domain_t<scalar_t>;
        if (nested_domain.empty()) return {};

        auto constexpr lowest = std::numeric_limits<scalar_t>::lowest();
        auto constexpr max = std::numeric_limits<scalar_t>::max();

        auto const first = bisect_first_true_t{}(lowest, max, [this, nested_domain](scalar_t input) noexcept {
            auto const output = try_apply(input);
            if (output) return *output >= nested_domain.first();
            return input > shift_;
        });
        if (!first) return {};

        auto const first_excluded = bisect_first_true_t{}(lowest, max, [this, nested_domain](scalar_t input) noexcept {
            auto const output = try_apply(input);
            if (output) return *output > nested_domain.last();
            return input > shift_;
        });

        if (first_excluded && *first_excluded <= *first) return {};
        auto const last = first_excluded ? std::nextafter(*first_excluded, lowest) : max;

        [[maybe_unused]] auto const first_output = try_apply(*first);
        [[maybe_unused]] auto const last_output = try_apply(last);
        assert(first_output && last_output && nested_domain.contains(*first_output)
            && nested_domain.contains(*last_output)
            && "resolved input-domain endpoints must map into the nested domain");

        return domain_t{*first, last};
    }

    [[nodiscard]] auto try_inverse(scalar_t input) const noexcept -> std::optional<scalar_t>
    {
        assert(std::isfinite(input) && "input_affine_t: inverse input must be finite");

        auto constexpr lowest = std::numeric_limits<scalar_t>::lowest();
        auto constexpr max = std::numeric_limits<scalar_t>::max();

        if (scale_ < scalar_t{1})
        {
            if (input > scalar_t{0} && input > max * scale_) return std::nullopt;
            if (input < scalar_t{0} && input < lowest * scale_) return std::nullopt;
        }
        auto const quotient = input / scale_;

        if (shift_ > scalar_t{0} && quotient > max - shift_) return std::nullopt;
        if (shift_ < scalar_t{0} && quotient < lowest - shift_) return std::nullopt;

        auto const output = shift_ + quotient;
        if (!std::isfinite(output)) return std::nullopt;
        return output;
    }

private:
    constexpr input_affine_t(scalar_t scale, scalar_t shift) noexcept : scale_{scale}, shift_{shift} {}

    scalar_t scale_;
    scalar_t shift_;
};

} // namespace crv::shaping::transforms
