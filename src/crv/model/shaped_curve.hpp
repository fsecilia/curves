// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/model/curves/curves.hpp>
#include <crv/tuple.hpp>
#include <crv/variant.hpp>
#include <utility>

namespace crv::model::curves {

template <typename scalar_t> struct evaluator_extractor_t
{
    template <typename curve_t> using op_t = typename curve_t::template evaluator_t<scalar_t>;
};

template <typename scalar_t>
using evaluator_variant_t
    = variant::to_variant_t<tuple::transform_t<curves::curves_t, evaluator_extractor_t<scalar_t>::template op_t>>;

template <typename scalar_t, typename config_t>
constexpr auto create_evaluator(config_t config) noexcept -> evaluator_variant_t<scalar_t>
{
    return {std::move(config)};
}

} // namespace crv::model::curves
