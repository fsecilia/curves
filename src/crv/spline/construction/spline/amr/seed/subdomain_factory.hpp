// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/fixed/float_conversions.hpp>
#include <crv/math/jet/jet.hpp>
#include <crv/spline/construction/segment/amr/transfer_sample.hpp>
#include <numeric>

namespace crv::spline::seed {

/// creates a subdomain directly between two exact fixed-point endpoints
template <is_fixed t_x_t, typename t_subdomain_t> struct subdomain_factory_t
{
    using x_t = t_x_t;
    using subdomain_t = t_subdomain_t;

    using scalar_t = subdomain_t::scalar_t;
    using jet_t = subdomain_t::jet_t;
    using function_sample_t = subdomain_t::function_sample_t;

    static constexpr auto midpoint(x_t left, x_t right) noexcept -> x_t
    {
        return x_t::literal(std::midpoint(left.value, right.value));
    }

    static constexpr auto operator()(
        auto const& target, function_sample_t const& left_sample, x_t left, x_t right) noexcept -> subdomain_t
    {
        assert(left < right && "subdomain endpoints must be strictly increasing");

        auto const midpoint_x = midpoint(left, right);
        auto const midpoint_sample = sample_transfer(target, jet_t{from_fixed<scalar_t>(midpoint_x), scalar_t{1}});
        auto const right_sample = sample_transfer(target, jet_t{from_fixed<scalar_t>(right), scalar_t{1}});

        return {
            .left_x = left,
            .midpoint_x = midpoint_x,
            .right_x = right,
            .left = left_sample,
            .midpoint = midpoint_sample,
            .right = right_sample,
        };
    }
};

} // namespace crv::spline::seed
