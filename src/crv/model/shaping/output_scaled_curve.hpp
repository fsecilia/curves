// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/model/curves/concepts.hpp>
#include <utility>
#include <vector>

namespace crv::shaping {

/// composes an output scale over a nested curve
template <typename t_transform_t, typename t_nested_curve_t>
    requires is_curve<t_nested_curve_t, typename t_nested_curve_t::scalar_t>
class output_scaled_curve_t
{
public:
    using transform_t = t_transform_t;
    using nested_curve_t = t_nested_curve_t;
    using scalar_t = nested_curve_t::scalar_t;
    using domain_t = nested_curve_t::domain_t;

    constexpr output_scaled_curve_t(transform_t transform, nested_curve_t curve) noexcept
        : transform_{std::move(transform)}, curve_{std::move(curve)}
    {}

    template <typename input_t> [[nodiscard]] auto operator()(input_t input) const noexcept -> input_t
    {
        return transform_.apply(curve_(input));
    }

    /// output scaling leaves the nested input domain unchanged
    [[nodiscard]] auto domain() const noexcept -> domain_t { return curve_.domain(); }

    /// output scaling leaves x-space critical points unchanged
    [[nodiscard]] auto critical_points() const -> std::vector<scalar_t> { return curve_.critical_points(); }

private:
    transform_t transform_;
    nested_curve_t curve_;
};

} // namespace crv::shaping
