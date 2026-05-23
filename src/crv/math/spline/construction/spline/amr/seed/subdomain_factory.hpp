// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/fixed/float_conversions.hpp>
#include <crv/math/int_traits.hpp>
#include <crv/math/jet/jet.hpp>
#include <bit>

namespace crv::spline::seed {

/// creates subdomains used to seed the refinement pool
///
/// This differs from a standard factory because of what information is available during seeding. The generated
/// subdomains are sequential, so the left sample is the previous subdomain's right sample. Widths are determined by a
/// dyadic stride rather than by pure bisection, so log2_width is not known and must be calculated.
template <is_fixed t_x_t, typename t_subdomain_t> struct subdomain_factory_t
{
    using x_t = t_x_t;
    using subdomain_t = t_subdomain_t;

    using scalar_t = subdomain_t::scalar_t;
    using jet_t = subdomain_t::jet_t;
    using function_sample_t = subdomain_t::function_sample_t;
    using signed_t = x_t::value_t;
    using unsigned_t = make_unsigned_t<signed_t>;

    static constexpr auto operator()(auto const& sample_target_function, function_sample_t const& left_sample,
        x_t const& left, x_t const& stride) noexcept -> subdomain_t
    {
        auto const right = left + stride;
        auto const midpoint = (left + right) / 2;

        auto const right_sample = sample_target_function(jet_t{from_fixed<scalar_t>(right), scalar_t{1}});
        auto const midpoint_sample = sample_target_function(jet_t{from_fixed<scalar_t>(midpoint), scalar_t{1}});

        auto const log2_width = std::countr_zero(static_cast<unsigned_t>(stride.value)) - x_t::frac_bits;

        return {
            .left = left_sample,
            .midpoint = midpoint_sample,
            .right = right_sample,
            .log2_width = log2_width,
        };
    }
};

} // namespace crv::spline::seed
