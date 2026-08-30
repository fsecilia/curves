// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/scalar_traits.hpp>
#include <crv/model/curves/concepts.hpp>
#include <cassert>
#include <cmath>
#include <utility>
#include <vector>

namespace crv::shaping {

/// composes a domain warp over a nested curve
template <typename t_transform_t, typename t_nested_curve_t>
    requires is_curve<t_nested_curve_t, typename t_nested_curve_t::scalar_t>
class domain_warp_curve_t
{
public:
    using transform_t = t_transform_t;
    using nested_curve_t = t_nested_curve_t;
    using scalar_t = nested_curve_t::scalar_t;
    using nested_domain_t = nested_curve_t::domain_t;

    /// propagated input domain
    class domain_t
    {
    public:
        constexpr domain_t(transform_t transform, nested_domain_t nested_domain) noexcept
            : transform_{std::move(transform)}, nested_domain_{std::move(nested_domain)}
        {}

        [[nodiscard]] auto contains(scalar_t input) const noexcept -> bool
        {
            auto const nested_input = transform_.try_apply(input);
            return nested_input && nested_domain_.contains(*nested_input);
        }

    private:
        [[no_unique_address]] transform_t transform_;
        [[no_unique_address]] nested_domain_t nested_domain_;
    };

    constexpr domain_warp_curve_t(transform_t transform, nested_curve_t curve) noexcept
        : transform_{std::move(transform)}, curve_{std::move(curve)}
    {}

    template <typename input_t> [[nodiscard]] auto operator()(input_t input) const noexcept -> input_t
    {
        assert(domain().contains(primal(input)) && "domain_warp_curve_t: input outside domain");
        return transform_.apply(curve_, input);
    }

    [[nodiscard]] auto domain() const noexcept -> domain_t { return {transform_, curve_.domain()}; }

    /// structural warp points plus reachable nested critical-point preimages
    [[nodiscard]] auto critical_points() const -> std::vector<scalar_t>
    {
        auto result = transform_.critical_points();
        auto const nested_points = curve_.critical_points();
        result.reserve(result.size() + nested_points.size());

        for (auto const point : nested_points)
        {
            assert(std::isfinite(point) && "domain_warp_curve_t: nested critical point must be finite");
            if (auto const preimage = transform_.try_preimage_critical_point(point)) result.push_back(*preimage);
        }
        return result;
    }

private:
    transform_t transform_;
    nested_curve_t curve_;
};

} // namespace crv::shaping
