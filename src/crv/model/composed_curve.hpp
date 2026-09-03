// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/model/curves/curves.hpp>
#include <crv/model/shaping/curve_evaluator.hpp>
#include <crv/model/shaping/shaped_curve.hpp>
#include <crv/tuple.hpp>
#include <crv/variant.hpp>

namespace crv::model::curves {

template <typename scalar_t> struct composed_curve_type_transform_t
{
    template <typename curve_t> using evaluator_t = curve_t::template evaluator_t<scalar_t>;
    template <typename curve_t> using evaluated_curve_t = shaping::curve_evaluator_t<evaluator_t<curve_t>>;
    template <typename curve_t> using shaped_curve_t = shaping::shaped_curve_builder_t::result_t<curve_t>;
    template <typename curve_t> using result_t = shaped_curve_t<evaluated_curve_t<curve_t>>;
};

template <typename scalar_t>
using composed_curve_variant_t = variant::to_variant_t<
    tuple::transform_t<curves::curves_t, composed_curve_type_transform_t<scalar_t>::template result_t>>;

template <typename variant_t> struct composed_curve_t
{
    variant_t variant;

    template <typename scalar_t> constexpr auto operator()(scalar_t input) const noexcept -> scalar_t
    {
        return std::visit([&](auto const& curve) { return curve(input); }, variant);
    }
};

template <typename scalar_t, typename config_t>
constexpr auto create_composed_curve(config_t config) noexcept -> composed_curve_t<composed_curve_variant_t<scalar_t>>
{
    using curve_t = config_t::curve_t;
    using evaluator_t = curve_t::template evaluator_t<scalar_t>;
    using evaluated_curve_t = shaping::curve_evaluator_t<evaluator_t>;
    return composed_curve_t<composed_curve_variant_t<scalar_t>>{
        shaping::shaped_curve_builder_t{}(evaluated_curve_t{evaluator_t{to_params<scalar_t>(config)}})};
}

} // namespace crv::model::curves
