// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/integer.hpp>
#include <bit>

namespace crv::spline::seed {

/// decomposes a seed span into a sequence of dyadic intervals
template <typename stride_calculator_t, typename subdomain_factory_t, typename interval_factory_t,
    int_t max_segment_count, int_t log2_min_width>
struct span_decomposer_t
{
    using x_t = subdomain_factory_t::x_t;
    using scalar_t = subdomain_factory_t::scalar_t;
    using jet_t = subdomain_factory_t::jet_t;
    using function_sample_t = subdomain_factory_t::function_sample_t;
    using unsigned_t = subdomain_factory_t::unsigned_t;

    [[no_unique_address]] stride_calculator_t calculate_stride;
    [[no_unique_address]] subdomain_factory_t create_subdomain;
    [[no_unique_address]] interval_factory_t create_interval;

    constexpr auto operator()(auto const& sample_target_function, function_sample_t left_sample, x_t left,
        x_t const& right, auto& refinement_pool) const -> function_sample_t
    {
        static constexpr auto align_shift = int_cast<int_t>(x_t::frac_bits + log2_min_width);
        static_assert(align_shift >= 0, "x_t precision cannot represent log2_min_width");

        assert(left < right && "critical points must be unique and strictly monotonically increasing");
        assert((static_cast<unsigned_t>(right.value) & ((unsigned_t{1} << align_shift) - 1)) == 0
            && "critical point not aligned to min segment width");

        // proceed in strides until subdomains cover span
        while (left < right)
        {
            assert(refinement_pool.size() < max_segment_count && "critical point partitioning exceeded segment budget");

            auto const stride = calculate_stride(left, right);
            assert(std::has_single_bit(static_cast<unsigned_t>(stride.value)) && "stride must be dyadic");
            assert(stride.value >= 2 && "stride midpoint must be representable");
            assert(stride >= (log2_min_width >= 0 ? (x_t{1} << log2_min_width) : (x_t{1} >> -log2_min_width))
                && "stride must be at least min segment width");
            assert(left + stride <= right && "stride must not exceed right boundary");

            auto const subdomain = create_subdomain(sample_target_function, left_sample, left, stride);
            refinement_pool.emplace(create_interval(sample_target_function, subdomain));

            left += stride;
            left_sample = subdomain.right;
        }

        return left_sample;
    }
};

} // namespace crv::spline::seed
