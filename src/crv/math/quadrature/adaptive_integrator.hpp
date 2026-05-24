// SPDX-License-Identifier: MIT

/// \file
/// \brief quadrature entrypoint
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/compensated_accumulator.hpp>
#include <crv/math/quadrature/antiderivative.hpp>
#include <crv/math/quadrature/bisector.hpp>
#include <crv/math/quadrature/segment.hpp>
#include <crv/math/quadrature/stack.hpp>
#include <crv/math/quadrature/subdivider.hpp>
#include <crv/ranges.hpp>
#include <cassert>
#include <type_traits>
#include <utility>
#include <vector>

namespace crv::quadrature {

template <typename integral_t> using antiderivative_of_t = antiderivative_t<std::remove_cvref_t<integral_t>>;
template <typename integral_t> using integration_result_of_t = integration_result_t<antiderivative_of_t<integral_t>>;

namespace generic {

/// top-level adaptive quadrature entrypoint
template <std::floating_point scalar_t, typename accumulator_t, typename subdivider_t, typename stack_seeder_t,
    typename bisector_t>
class adaptive_integrator_t
{
public:
    constexpr adaptive_integrator_t(scalar_t tolerance, int_t depth_limit, subdivider_t subdivider = {},
        stack_seeder_t stack_seeder = {}, bisector_t bisector = {})
        : subdivider_{std::move(subdivider)}, stack_seeder_{std::move(stack_seeder)}, bisector_{std::move(bisector)},
          tolerance_{tolerance}, depth_limit_{depth_limit}
    {
        stack_.reserve(32);
    }

    /// DI overload
    template <typename integral_t, typename antiderivative_builder_t>
    constexpr auto operator()(integral_t integral, antiderivative_builder_t antiderivative_builder, scalar_t domain_end,
        compatible_range<scalar_t> auto const& critical_points) -> typename antiderivative_builder_t::result_t
    {
        // TODO: Automatically clearing the stack here prevents issues if a previous run threw an exception. However,
        // this becomes a slippery slope when we support incremental evaluation or resuming an interrupted integration.
        // When those features are added, this clear must be removed and replaced with explicit lifecycle management by
        // the caller.
        stack_.clear();

        stack_seeder_.seed(stack_, integral, domain_end, tolerance_, critical_points);
        subdivider_.run(stack_, integral, bisector_, antiderivative_builder, depth_limit_);

        return std::move(antiderivative_builder).finalize(std::move(integral));
    }

    /// prod overload
    template <typename integral_t>
    constexpr auto operator()(integral_t integral, scalar_t domain_end,
        compatible_range<scalar_t> auto const& critical_points) -> integration_result_of_t<integral_t>
    {
        using antiderivative_t = antiderivative_t<integral_t>;
        using antiderivative_builder_t = antiderivative_builder_t<accumulator_t, antiderivative_t>;
        return operator()(std::move(integral), antiderivative_builder_t{}, domain_end, critical_points);
    }

private:
    using segment_t = segment_t<scalar_t>;
    using stack_t = std::vector<segment_t>;

    [[no_unique_address]] subdivider_t subdivider_;
    [[no_unique_address]] stack_seeder_t stack_seeder_;
    [[no_unique_address]] bisector_t bisector_;
    stack_t stack_;
    scalar_t tolerance_;
    int_t depth_limit_;
};

} // namespace generic

template <std::floating_point scalar_t>
using adaptive_integrator_t = generic::adaptive_integrator_t<scalar_t, compensated_accumulator_t<scalar_t>,
    subdivider_t<scalar_t>, stack_seeder_t<scalar_t>, bisector_t>;

} // namespace crv::quadrature
