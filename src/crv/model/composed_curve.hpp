// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/model/curves/curves.hpp>
#include <crv/model/shaping/shaped_curve.hpp>
#include <crv/tuple.hpp>
#include <crv/variant.hpp>
#include <utility>

namespace crv::model::curves {

template <typename scalar_t> struct composed_curve_type_transform_t
{
    template <typename curve_t> using curve_evaluator_t = typename curve_t::template evaluator_t<scalar_t>;
    template <typename curve_t> using shaped_curve_t = shaping::shaped_curve_builder_t::result_t<curve_t>;
    template <typename curve_t> using result_t = shaped_curve_t<curve_evaluator_t<curve_t>>;
};

template <typename scalar_t>
using composed_curve_variant_t = variant::to_variant_t<
    tuple::transform_t<curves::curves_t, composed_curve_type_transform_t<scalar_t>::template result_t>>;

template <typename scalar_t, typename config_t>
constexpr auto create_composed_curve(config_t config) noexcept -> composed_curve_variant_t<scalar_t>
{
    using curve_t = config_t::curve_t;
    using evaluator_t = curve_t::template evaluator_t<scalar_t>;
    return shaping::shaped_curve_builder_t{}(evaluator_t{to_params(config)});
}

} // namespace crv::model::curves
