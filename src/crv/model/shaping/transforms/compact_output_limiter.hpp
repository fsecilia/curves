// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/jet/jet.hpp>
#include <crv/model/shaping/transitions/concepts.hpp>
#include <cassert>
#include <cmath>
#include <concepts>
#include <expected>
#include <limits>
#include <optional>
#include <utility>

namespace crv::shaping::transforms {

enum class output_limiter_error_t : uint8_t
{
    bound_not_finite,
    bound_negative,
    delta_y_not_finite,
    delta_y_negative,
    zero_bound_requires_zero_delta_y,
    positive_bound_requires_positive_delta_y,
    upper_delta_y_not_below_bound,
    transition_half_integral_not_finite,
    transition_half_integral_not_positive,
    log_half_width_not_finite,
    log_half_width_not_positive,
    log_support_not_finite,
};

enum class output_limiter_region_t : uint8_t
{
    plateau,
    transition,
    identity,
};

enum class fixed_anchor_feasibility_t : uint8_t
{
    feasible,
    transition_conflict,
    bound_conflict,
};

template <std::floating_point t_scalar_t> struct output_limiter_log_support_t
{
    using scalar_t = t_scalar_t;

    scalar_t log_bound;
    scalar_t half_width;
    scalar_t lower_log;
    scalar_t upper_log;

    constexpr auto operator==(output_limiter_log_support_t const&) const noexcept -> bool = default;
};

namespace detail {

enum class output_limiter_side_t : uint8_t
{
    lower,
    upper,
};

/// limiter running in output space with compact support
template <std::floating_point t_scalar_t, typename t_transition_t, output_limiter_side_t side>
    requires transitions::is_transition<t_transition_t, t_scalar_t>
class compact_output_limiter_t
{
public:
    using scalar_t = t_scalar_t;
    using transition_t = t_transition_t;
    using jet_t = crv::jet_t<scalar_t>;
    using log_support_t = output_limiter_log_support_t<scalar_t>;
    using construction_error_t = output_limiter_error_t;
    using construction_result_t = std::expected<compact_output_limiter_t, construction_error_t>;

    [[nodiscard]] static auto make(scalar_t bound, scalar_t delta_y, transition_t transition) -> construction_result_t
    {
        if (!std::isfinite(bound)) return std::unexpected{construction_error_t::bound_not_finite};
        if (bound < scalar_t{0}) return std::unexpected{construction_error_t::bound_negative};
        if (!std::isfinite(delta_y)) return std::unexpected{construction_error_t::delta_y_not_finite};
        if (delta_y < scalar_t{0}) return std::unexpected{construction_error_t::delta_y_negative};

        if (bound == scalar_t{0})
        {
            if (delta_y != scalar_t{0})
            {
                return std::unexpected{construction_error_t::zero_bound_requires_zero_delta_y};
            }
            return compact_output_limiter_t{
                bound, delta_y, std::move(transition), std::nullopt, scalar_t{0}, scalar_t{0}};
        }

        if (delta_y == scalar_t{0})
        {
            return std::unexpected{construction_error_t::positive_bound_requires_positive_delta_y};
        }
        if constexpr (side == output_limiter_side_t::upper)
        {
            if (delta_y >= bound) return std::unexpected{construction_error_t::upper_delta_y_not_below_bound};
        }

        auto const half_integral = transition.antiderivative(scalar_t{0.5});
        if (!std::isfinite(half_integral))
        {
            return std::unexpected{construction_error_t::transition_half_integral_not_finite};
        }
        if (half_integral <= scalar_t{0})
        {
            return std::unexpected{construction_error_t::transition_half_integral_not_positive};
        }

        auto const half_width = derive_half_width(bound, delta_y, half_integral);
        if (!std::isfinite(half_width)) return std::unexpected{construction_error_t::log_half_width_not_finite};
        if (half_width <= scalar_t{0}) return std::unexpected{construction_error_t::log_half_width_not_positive};

        auto const log_bound = std::log(bound);
        auto const lower_log = log_bound - half_width;
        auto const upper_log = log_bound + half_width;
        if (!std::isfinite(log_bound) || !std::isfinite(lower_log) || !std::isfinite(upper_log))
        {
            return std::unexpected{construction_error_t::log_support_not_finite};
        }

        return compact_output_limiter_t{bound, delta_y, std::move(transition),
            log_support_t{
                .log_bound = log_bound,
                .half_width = half_width,
                .lower_log = lower_log,
                .upper_log = upper_log,
            },
            std::exp(lower_log), std::exp(upper_log)};
    }

    [[nodiscard]] auto operator()(scalar_t output) const noexcept -> scalar_t
    {
        auto const region = classify(output);
        if (region == output_limiter_region_t::plateau) return bound_;
        if (region == output_limiter_region_t::identity) return output;
        return transition_value_from_output(output);
    }

    [[nodiscard]] auto operator()(jet_t output) const noexcept -> jet_t
    {
        auto const value = primal(output);
        auto const region = classify(value);
        if (region == output_limiter_region_t::plateau) return jet_t{bound_};
        if (region == output_limiter_region_t::identity) return output;

        auto const log_output = std::log(value);
        auto const u = normalized_coordinate(log_output);
        auto const limited = transition_value_from_u(u);
        auto const multiplier = limited / value * transition_.value(u);
        return {limited, tangent(output) * multiplier};
    }

    [[nodiscard]] auto classify(scalar_t output) const noexcept -> output_limiter_region_t
    {
        assert(std::isfinite(output) && output >= scalar_t{0}
            && "compact_output_limiter_t: output must be finite and nonnegative");
        if (!support_)
        {
            if constexpr (side == output_limiter_side_t::upper) return output_limiter_region_t::plateau;
            return output_limiter_region_t::identity;
        }

        if constexpr (side == output_limiter_side_t::upper)
        {
            if (output <= lower_output_) return output_limiter_region_t::identity;
            if (output >= upper_output_) return output_limiter_region_t::plateau;
        }
        else
        {
            if (output <= lower_output_) return output_limiter_region_t::plateau;
            if (output >= upper_output_) return output_limiter_region_t::identity;
        }
        return output_limiter_region_t::transition;
    }

    [[nodiscard]] auto classify_log(scalar_t log_output) const noexcept -> output_limiter_region_t
    {
        assert(support_ && "compact_output_limiter_t: zero-bound limiter has no logarithmic support");
        if constexpr (side == output_limiter_side_t::upper)
        {
            if (log_output <= support_->lower_log) return output_limiter_region_t::identity;
            if (log_output >= support_->upper_log) return output_limiter_region_t::plateau;
        }
        else
        {
            if (log_output <= support_->lower_log) return output_limiter_region_t::plateau;
            if (log_output >= support_->upper_log) return output_limiter_region_t::identity;
        }
        return output_limiter_region_t::transition;
    }

    [[nodiscard]] constexpr auto bound() const noexcept -> scalar_t { return bound_; }
    [[nodiscard]] constexpr auto delta_y() const noexcept -> scalar_t { return delta_y_; }
    [[nodiscard]] constexpr auto support() const noexcept -> std::optional<log_support_t> const& { return support_; }

    [[nodiscard]] constexpr auto is_globally_constant() const noexcept -> bool
    {
        if constexpr (side == output_limiter_side_t::upper) return bound_ == scalar_t{0};
        return false;
    }

    [[nodiscard]] constexpr auto is_globally_identity() const noexcept -> bool
    {
        if constexpr (side == output_limiter_side_t::lower) return bound_ == scalar_t{0};
        return false;
    }

    [[nodiscard]] auto fixed_anchor_feasibility(scalar_t anchor) const noexcept -> fixed_anchor_feasibility_t
        requires(side == output_limiter_side_t::upper)
    {
        assert(std::isfinite(anchor) && anchor >= scalar_t{0}
            && "compact_output_limiter_t: anchor must be finite and nonnegative");
        if (anchor == scalar_t{0}) return fixed_anchor_feasibility_t::feasible;
        if (bound_ == scalar_t{0} || anchor > bound_) return fixed_anchor_feasibility_t::bound_conflict;
        if (std::log(anchor) <= support_->lower_log) return fixed_anchor_feasibility_t::feasible;
        return fixed_anchor_feasibility_t::transition_conflict;
    }

private:
    constexpr compact_output_limiter_t(scalar_t bound, scalar_t delta_y, transition_t transition,
        std::optional<log_support_t> support, scalar_t lower_output, scalar_t upper_output) noexcept
        : bound_{bound}, delta_y_{delta_y}, support_{std::move(support)}, lower_output_{lower_output},
          upper_output_{upper_output}, transition_{std::move(transition)}
    {}

    [[nodiscard]] static auto derive_half_width(scalar_t bound, scalar_t delta_y, scalar_t half_integral) noexcept
        -> scalar_t
    {
        if constexpr (side == output_limiter_side_t::upper)
        {
            return -std::log1p(-delta_y / bound) / (scalar_t{2} * half_integral);
        }

        auto const ratio = delta_y / bound;
        auto const log_ratio_plus_one = std::isfinite(ratio)
            ? std::log1p(ratio)
            : std::log(delta_y) - std::log(bound) + std::log1p(bound / delta_y);
        return log_ratio_plus_one / (scalar_t{2} * half_integral);
    }

    [[nodiscard]] auto normalized_coordinate(scalar_t log_output) const noexcept -> scalar_t
    {
        if constexpr (side == output_limiter_side_t::upper)
        {
            return (support_->upper_log - log_output) / (scalar_t{2} * support_->half_width);
        }
        return (log_output - support_->lower_log) / (scalar_t{2} * support_->half_width);
    }

    [[nodiscard]] auto transition_value_from_output(scalar_t output) const noexcept -> scalar_t
    {
        auto const log_output = std::log(output);
        return transition_value_from_u(normalized_coordinate(log_output));
    }

    [[nodiscard]] auto transition_value_from_u(scalar_t u) const noexcept -> scalar_t
    {
        auto const integral = transition_.antiderivative(u);
        auto const exponent = scalar_t{2} * support_->half_width * integral;
        if constexpr (side == output_limiter_side_t::upper) return bound_ * std::exp(-exponent);

        if (exponent <= std::log(std::numeric_limits<scalar_t>::max())) return bound_ * std::exp(exponent);
        return std::exp(support_->log_bound + exponent);
    }

    scalar_t bound_;
    scalar_t delta_y_;
    std::optional<log_support_t> support_;
    scalar_t lower_output_;
    scalar_t upper_output_;
    [[no_unique_address]] transition_t transition_;
};

} // namespace detail

template <std::floating_point scalar_t, typename transition_t>
using upper_output_limiter_t
    = detail::compact_output_limiter_t<scalar_t, transition_t, detail::output_limiter_side_t::upper>;

template <std::floating_point scalar_t, typename transition_t>
using lower_output_limiter_t
    = detail::compact_output_limiter_t<scalar_t, transition_t, detail::output_limiter_side_t::lower>;

} // namespace crv::shaping::transforms
