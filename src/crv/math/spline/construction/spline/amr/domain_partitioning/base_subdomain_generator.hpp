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

namespace crv::spline {

/// generates a subdomain from a stride
///
/// This type generates subdomains for intervals that seed the refinement pool. It uses a stride calculator to determine
/// where to place the next subdomain given the left x and the target x.
template <typename t_subdomain_t, typename stride_calculator_t> struct base_subdomain_generator_t
{
    using subdomain_t = t_subdomain_t;

    using scalar_t = subdomain_t::scalar_t;
    using jet_t = subdomain_t::jet_t;
    using function_sample_t = subdomain_t::function_sample_t;
    using x_t = stride_calculator_t::x_t;

    using signed_t = x_t::value_t;
    using unsigned_t = make_unsigned_t<signed_t>;

    struct result_t
    {
        subdomain_t subdomain;
        x_t next_x;

        constexpr auto operator==(result_t const&) const noexcept -> bool = default;
    };

    [[no_unique_address]] stride_calculator_t calculate_stride;

    constexpr auto operator()(auto const& sample_target_function, function_sample_t const& current_sample,
        x_t const& current_x, x_t const& target_x) const -> result_t
    {
        auto const stride = calculate_stride(current_x, target_x);

        assert(std::has_single_bit(static_cast<unsigned_t>(stride.value)) && "stride must be dyadic");
        assert(stride.value >= 2 && "stride midpoint must be representable");

        auto const next_x = current_x + stride;
        auto const midpoint_x = current_x + (stride >> 1);

        auto const right_sample = sample_target_function(jet_t{from_fixed<scalar_t>(next_x), scalar_t{1}});
        auto const midpoint_sample = sample_target_function(jet_t{from_fixed<scalar_t>(midpoint_x), scalar_t{1}});

        return result_t{
            .subdomain = subdomain_t{
                .left = current_sample,
                .midpoint = midpoint_sample,
                .right = right_sample,
                .log2_width = std::countr_zero(static_cast<unsigned_t>(stride.value)) - x_t::frac_bits,
            },
            .next_x = next_x
        };
    }
};

} // namespace crv::spline
