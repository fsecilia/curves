// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/scalar_traits.hpp>
#include <crv/model/curves/concepts.hpp>
#include <crv/model/domain.hpp>
#include <cassert>
#include <cmath>
#include <utility>
#include <vector>

namespace crv::shaping {

/// composes an input transform over a curve
template <typename t_transform_t, typename t_nested_curve_t>
    requires is_curve<t_nested_curve_t, typename t_nested_curve_t::scalar_t>
class input_affine_curve_t
{
public:
    using transform_t = t_transform_t;
    using nested_curve_t = t_nested_curve_t;
    using scalar_t = nested_curve_t::scalar_t;

    constexpr input_affine_curve_t(transform_t transform, nested_curve_t curve) noexcept
        : transform_{std::move(transform)}, curve_{std::move(curve)},
          input_domain_{transform_.preimage(curve_.input_domain())}
    {}

    template <typename input_t> [[nodiscard]] auto operator()(input_t input) const noexcept -> input_t
    {
        assert(input_domain_.contains(primal(input)) && "input_affine_curve_t: input outside domain");
        return curve_(transform_.apply(input));
    }

    [[nodiscard]] constexpr auto input_domain() const noexcept -> model::input_domain_t<scalar_t>
    {
        return input_domain_;
    }

    /// nested critical points inverse-transformed into this curve's input coordinate
    [[nodiscard]] auto critical_points() const -> std::vector<scalar_t>
    {
        auto const nested_points = curve_.critical_points();
        auto result = std::vector<scalar_t>{};
        result.reserve(nested_points.size());
        for (auto const point : nested_points)
        {
            assert(std::isfinite(point) && "input_affine_curve_t: nested critical point must be finite");
            if (auto const transformed = transform_.try_inverse(point)) result.push_back(*transformed);
        }
        return result;
    }

private:
    transform_t transform_;
    nested_curve_t curve_;
    model::input_domain_t<scalar_t> input_domain_;
};

} // namespace crv::shaping
