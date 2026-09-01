// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/inverse.hpp>
#include <crv/math/jet/jet.hpp>
#include <crv/model/domain.hpp>
#include <crv/model/shaping/transitions/concepts.hpp>
#include <cassert>
#include <cmath>
#include <concepts>
#include <expected>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

namespace crv::shaping::transforms {

enum class domain_warp_error_t : uint8_t
{
    hold_width_not_finite,
    hold_width_negative,
    transition_width_not_finite,
    transition_width_negative,
    support_not_representable,
    transition_endpoint_integral_not_finite,
    transition_endpoint_integral_not_positive,
    transition_endpoint_integral_above_one,
};

/// exact input hold followed by a smooth release and permanent horizontal lag
template <std::floating_point t_scalar_t, typename t_transition_t>
    requires transitions::is_transition<t_transition_t, t_scalar_t>
class domain_warp_t
{
public:
    using scalar_t = t_scalar_t;
    using transition_t = t_transition_t;
    using jet_t = crv::jet_t<scalar_t>;
    using construction_result_t = std::expected<domain_warp_t, domain_warp_error_t>;

    [[nodiscard]] static auto make(scalar_t hold_width, scalar_t transition_width, transition_t transition)
        -> construction_result_t
    {
        if (!std::isfinite(hold_width)) return std::unexpected{domain_warp_error_t::hold_width_not_finite};
        if (hold_width < scalar_t{0}) return std::unexpected{domain_warp_error_t::hold_width_negative};
        if (!std::isfinite(transition_width))
        {
            return std::unexpected{domain_warp_error_t::transition_width_not_finite};
        }
        if (transition_width < scalar_t{0}) return std::unexpected{domain_warp_error_t::transition_width_negative};

        if (transition_width == scalar_t{0})
        {
            return domain_warp_t{hold_width, transition_width, std::move(transition), std::nullopt};
        }

        auto const support_end = hold_width + transition_width;
        if (!std::isfinite(support_end) || support_end <= hold_width)
        {
            return std::unexpected{domain_warp_error_t::support_not_representable};
        }

        auto const endpoint_integral = transition.antiderivative(scalar_t{1});
        if (!std::isfinite(endpoint_integral))
        {
            return std::unexpected{domain_warp_error_t::transition_endpoint_integral_not_finite};
        }
        if (endpoint_integral <= scalar_t{0})
        {
            return std::unexpected{domain_warp_error_t::transition_endpoint_integral_not_positive};
        }
        if (endpoint_integral > scalar_t{1})
        {
            return std::unexpected{domain_warp_error_t::transition_endpoint_integral_above_one};
        }

        auto const transition_output_end = transition_width * endpoint_integral;
        auto const lag = support_end - transition_output_end;
        return domain_warp_t{hold_width, transition_width, std::move(transition),
            transition_geometry_t{
                .support_end = support_end,
                .transition_output_end = transition_output_end,
                .lag = lag,
            }};
    }

    [[nodiscard]] auto try_apply(scalar_t input) const noexcept -> std::optional<scalar_t>
    {
        if (!std::isfinite(input)) return std::nullopt;

        auto const output = apply_unchecked(input);
        if (!std::isfinite(output) || output < scalar_t{0}) return std::nullopt;
        return output;
    }

    [[nodiscard]] auto apply(scalar_t input) const noexcept -> scalar_t
    {
        auto const output = try_apply(input);
        assert(output.has_value() && "domain_warp_t: transformed scalar must be finite and representable");
        return *output;
    }

    [[nodiscard]] auto apply(jet_t input) const noexcept -> jet_t
    {
        auto const input_value = primal(input);
        assert(std::isfinite(input_value) && "domain_warp_t: jet primal must be finite");

        auto const region = classify(input_value);
        if (region == region_t::hold) return jet_t{scalar_t{0}};
        if (region == region_t::release_endpoint) return {geometry_->transition_output_end, tangent(input)};
        if (region == region_t::progression) return {input_value - progression_lag(), tangent(input)};

        auto const u = (input_value - hold_width_) / transition_width_;
        auto const output = transition_width_ * transition_.antiderivative(u);
        assert(std::isfinite(output) && output >= scalar_t{0}
            && "domain_warp_t: transition output must be finite and nonnegative");
        return {output, transition_.value(u) * tangent(input)};
    }

    template <typename curve_t>
    [[nodiscard]] auto apply(curve_t const& curve, scalar_t input) const noexcept -> scalar_t
    {
        return curve(apply(input));
    }

    template <typename curve_t> [[nodiscard]] auto apply(curve_t const& curve, jet_t input) const noexcept -> jet_t
    {
        auto const input_value = primal(input);
        assert(std::isfinite(input_value) && "domain_warp_t: jet primal must be finite");

        if (classify(input_value) == region_t::hold) return {curve(scalar_t{0}), scalar_t{0}};
        return curve(apply(input));
    }

    /// exact outer-input preimage of a nested input domain
    [[nodiscard]] auto preimage(model::input_domain_t<scalar_t> nested_domain) const noexcept
        -> model::input_domain_t<scalar_t>
    {
        using domain_t = model::input_domain_t<scalar_t>;
        if (nested_domain.empty() || nested_domain.last() < scalar_t{0}) return {};

        auto constexpr lowest = std::numeric_limits<scalar_t>::lowest();
        auto constexpr max = std::numeric_limits<scalar_t>::max();

        auto first = std::optional<scalar_t>{lowest};
        if (nested_domain.first() > scalar_t{0}) first = first_at_least(nested_domain.first());
        if (!first) return {};

        auto const first_excluded = first_above(nested_domain.last());
        if (first_excluded && *first_excluded <= *first) return {};
        auto const last = first_excluded ? std::nextafter(*first_excluded, lowest) : max;

        auto const first_output = try_apply(*first);
        auto const last_output = try_apply(last);
        assert(first_output && last_output && nested_domain.contains(*first_output)
            && nested_domain.contains(*last_output)
            && "resolved input-domain endpoints must map into the nested domain");
        return domain_t{*first, last};
    }

    /// structural hold/release boundaries in the transform's input coordinate
    [[nodiscard]] auto critical_points() const -> std::vector<scalar_t>
    {
        if (!geometry_) return {hold_width_};
        return {hold_width_, geometry_->support_end};
    }

    /// unique finite preimage for a reachable positive nested critical point, when representable
    [[nodiscard]] auto try_preimage_critical_point(scalar_t nested_point) const noexcept -> std::optional<scalar_t>
    {
        assert(std::isfinite(nested_point) && "domain_warp_t: nested critical point must be finite");
        if (nested_point <= scalar_t{0}) return std::nullopt;

        if (!geometry_) return try_add_after_boundary(hold_width_, nested_point, hold_width_);

        if (nested_point == geometry_->transition_output_end) return geometry_->support_end;
        if (nested_point > geometry_->transition_output_end)
        {
            return try_add_after_boundary(nested_point, geometry_->lag, geometry_->support_end);
        }

        auto const preimage = bisect_lower_bound_t{}(hold_width_, geometry_->support_end, nested_point,
            [this](scalar_t input) noexcept { return apply_unchecked(input); });
        return !preimage || *preimage <= hold_width_ || *preimage >= geometry_->support_end ? std::nullopt : preimage;
    }

private:
    enum class region_t : uint8_t
    {
        hold,
        transition,
        release_endpoint,
        progression,
    };

    struct transition_geometry_t
    {
        scalar_t support_end;
        scalar_t transition_output_end;
        scalar_t lag;
    };

    constexpr domain_warp_t(scalar_t hold_width, scalar_t transition_width, transition_t transition,
        std::optional<transition_geometry_t> geometry) noexcept
        : hold_width_{hold_width}, transition_width_{transition_width}, transition_{std::move(transition)},
          geometry_{std::move(geometry)}
    {}

    [[nodiscard]] auto classify(scalar_t input) const noexcept -> region_t
    {
        if (input <= hold_width_) return region_t::hold;
        if (!geometry_) return region_t::progression;
        if (input < geometry_->support_end) return region_t::transition;
        if (input == geometry_->support_end) return region_t::release_endpoint;
        return region_t::progression;
    }

    [[nodiscard]] auto first_at_least(scalar_t target) const noexcept -> std::optional<scalar_t>
    {
        assert(std::isfinite(target) && target > scalar_t{0} && "domain_warp_t: invalid lower target");
        auto const max = std::numeric_limits<scalar_t>::max();

        if (!geometry_)
        {
            return bisect_first_true_t{}(
                hold_width_, max, [this, target](scalar_t input) noexcept { return apply_unchecked(input) >= target; });
        }

        if (target <= geometry_->transition_output_end)
        {
            return bisect_first_true_t{}(hold_width_, geometry_->support_end,
                [this, target](scalar_t input) noexcept { return apply_unchecked(input) >= target; });
        }

        return bisect_first_true_t{}(geometry_->support_end, max,
            [this, target](scalar_t input) noexcept { return apply_unchecked(input) >= target; });
    }

    [[nodiscard]] auto first_above(scalar_t target) const noexcept -> std::optional<scalar_t>
    {
        assert(std::isfinite(target) && "domain_warp_t: invalid upper target");
        auto const max = std::numeric_limits<scalar_t>::max();

        if (!geometry_)
        {
            return bisect_first_true_t{}(
                hold_width_, max, [this, target](scalar_t input) noexcept { return apply_unchecked(input) > target; });
        }

        if (target < geometry_->transition_output_end)
        {
            return bisect_first_true_t{}(hold_width_, geometry_->support_end,
                [this, target](scalar_t input) noexcept { return apply_unchecked(input) > target; });
        }

        return bisect_first_true_t{}(geometry_->support_end, max,
            [this, target](scalar_t input) noexcept { return apply_unchecked(input) > target; });
    }

    [[nodiscard]] auto progression_lag() const noexcept -> scalar_t
    {
        if (!geometry_) return hold_width_;
        return geometry_->lag;
    }

    [[nodiscard]] auto apply_unchecked(scalar_t input) const noexcept -> scalar_t
    {
        auto const region = classify(input);
        if (region == region_t::hold) return scalar_t{0};
        if (region == region_t::release_endpoint) return geometry_->transition_output_end;
        if (region == region_t::progression) return input - progression_lag();

        auto const u = (input - hold_width_) / transition_width_;
        return transition_width_ * transition_.antiderivative(u);
    }

    [[nodiscard]] static auto try_add_after_boundary(scalar_t left, scalar_t right, scalar_t boundary) noexcept
        -> std::optional<scalar_t>
    {
        auto const max = std::numeric_limits<scalar_t>::max();
        if (right > max - left) return std::nullopt;

        auto const result = left + right;
        if (!std::isfinite(result) || result <= boundary) return std::nullopt;
        return result;
    }

    scalar_t hold_width_;
    scalar_t transition_width_;
    [[no_unique_address]] transition_t transition_;
    std::optional<transition_geometry_t> geometry_;
};

} // namespace crv::shaping::transforms
