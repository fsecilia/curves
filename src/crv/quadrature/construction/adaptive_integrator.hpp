// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/quadrature/adaptive_integration_receipt.hpp>
#include <crv/quadrature/antiderivative.hpp>
#include <crv/quadrature/construction/segment.hpp>
#include <crv/ranges.hpp>
#include <cassert>
#include <cmath>
#include <concepts>
#include <type_traits>
#include <utility>
#include <vector>

namespace crv::quadrature::construction {

/// adaptive quadrature construction engine
template <std::floating_point t_scalar_t, typename t_cache_builder_factory_t, typename t_subdivider_t,
    typename t_stack_seeder_t>
class adaptive_integrator_t
{
public:
    using scalar_t = t_scalar_t;
    using cache_builder_factory_t = t_cache_builder_factory_t;
    using subdivider_t = t_subdivider_t;
    using stack_seeder_t = t_stack_seeder_t;

    using receipt_t = quadrature::adaptive_integration_receipt_t<scalar_t>;

    template <typename t_integral_t> struct result_t
    {
        using integral_t = std::remove_cvref_t<t_integral_t>;
        using antiderivative_t = quadrature::antiderivative_t<integral_t>;
        using receipt_t = quadrature::adaptive_integration_receipt_t<scalar_t>;

        antiderivative_t antiderivative;
        receipt_t receipt;
    };

    constexpr adaptive_integrator_t(
        cache_builder_factory_t cache_builder_factory, subdivider_t subdivider, stack_seeder_t stack_seeder)
        : cache_builder_factory_{std::move(cache_builder_factory)}, subdivider_{std::move(subdivider)},
          stack_seeder_{std::move(stack_seeder)}
    {
        stack_.reserve(32);
    }

    /// constructs an antiderivative and adaptive integration receipt
    template <typename integral_t>
    constexpr auto operator()(integral_t integral, scalar_t domain_end, scalar_t tolerance, int_t depth_limit,
        compatible_range<scalar_t> auto const& critical_points) const -> result_t<integral_t>
    {
        assert(std::isfinite(tolerance) && tolerance > scalar_t{0}
            && "adaptive_integrator_t: tolerance must be finite and positive");
        assert(depth_limit >= int_t{0} && "adaptive_integrator_t: depth limit must be nonnegative");

        auto cache_builder = cache_builder_factory_();

        // clear reusable stack before seeding
        //
        // This also makes a previous interrupted integration harmless to the next call.
        stack_.clear();

        stack_seeder_.seed(stack_, integral, domain_end, tolerance, critical_points);
        subdivider_.run(stack_, integral, cache_builder, depth_limit);

        auto cache_result = std::move(cache_builder).finalize();
        using antiderivative_t = quadrature::antiderivative_t<std::remove_cvref_t<integral_t>>;
        auto antiderivative = antiderivative_t{std::move(integral), std::move(cache_result.cache)};
        auto const receipt = receipt_t{
            .requested_tolerance = tolerance,
            .achieved_error = cache_result.achieved_error,
            .max_error = cache_result.max_error,
            .segment_count = antiderivative.segment_count(),
            .refinement_limited = cache_result.refinement_limited,
        };

        return {.antiderivative = std::move(antiderivative), .receipt = receipt};
    }

private:
    using segment_t = construction::segment_t<scalar_t>;
    using stack_t = std::vector<segment_t>;

    [[no_unique_address]] cache_builder_factory_t cache_builder_factory_;
    [[no_unique_address]] subdivider_t subdivider_;
    [[no_unique_address]] stack_seeder_t stack_seeder_;
    mutable stack_t stack_;
};

} // namespace crv::quadrature::construction
