// SPDX-License-Identifier: MIT

/// \file
/// \brief
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
template <typename integral_t> using result_of_t = integration_result_t<antiderivative_of_t<integral_t>>;

namespace generic {

/// top-level adaptive quadrature entrypoint
template <std::floating_point real_t, typename accumulator_t, typename subdivider_t, typename stack_seeder_t>
class adaptive_integrator_t
{
public:
    constexpr adaptive_integrator_t(
        real_t tolerance, int_t depth_limit, subdivider_t subdivider = {}, stack_seeder_t stack_seeder = {})
        : subdivider_{std::move(subdivider)}, stack_seeder_{std::move(stack_seeder)}, tolerance_{tolerance},
          depth_limit_{depth_limit}
    {
        stack_.reserve(32);
    }

    /// wide overload: full DI, every ephemeral injected
    template <typename bisector_t, typename antiderivative_builder_t>
    constexpr auto operator()(bisector_t bisector, antiderivative_builder_t antiderivative_builder, real_t domain_max,
        compatible_range<real_t> auto const& critical_points) -> typename antiderivative_builder_t::result_t
    {
        // TODO: Automatically clearing the stack here prevents issues if a previous run threw an exception. However,
        // this becomes a slippery slope when we support incremental evaluation or resuming and interrupted integration.
        // When those features are added, this clear must be removed and replaced with explicit lifecycle management by
        // the caller.
        stack_.clear();

        stack_seeder_.seed(stack_, bisector, domain_max, tolerance_, critical_points);
        subdivider_.run(stack_, bisector, antiderivative_builder, depth_limit_);

        return std::move(antiderivative_builder).finalize(std::move(bisector).finalize());
    }

    /// narrow overload: convenience, constructs ephemerals and delegates to wide
    template <typename integral_t>
    constexpr auto operator()(integral_t integral, real_t domain_max,
        compatible_range<real_t> auto const& critical_points) -> result_of_t<integral_t>
    {
        using antiderivative_t = antiderivative_t<integral_t>;
        using antiderivative_builder_t = antiderivative_builder_t<accumulator_t, antiderivative_t>;
        return operator()(bisector_t{std::move(integral)}, antiderivative_builder_t{}, domain_max, critical_points);
    }

    /// narrow overload: convenience, delegates with empty critial points
    template <typename integral_t>
    constexpr auto operator()(integral_t integral, real_t domain_max) -> result_of_t<integral_t>
    {
        return operator()(std::move(integral), domain_max, std::array<real_t, 0>{});
    }

private:
    using segment_t = segment_t<real_t>;
    using stack_t = std::vector<segment_t>;

    subdivider_t subdivider_;
    stack_seeder_t stack_seeder_;
    stack_t stack_;
    real_t tolerance_;
    int_t depth_limit_;
};

} // namespace generic

template <std::floating_point real_t>
using adaptive_integrator_t = generic::adaptive_integrator_t<real_t, compensated_accumulator_t<real_t>,
    subdivider_t<real_t>, stack_seeder_t<real_t>>;

} // namespace crv::quadrature
