// SPDX-License-Identifier: MIT

/// \file
/// \brief composition root for antiderivative construction
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/compensated_accumulator.hpp>
#include <crv/quadrature/adaptive_integration_receipt.hpp>
#include <crv/quadrature/antiderivative.hpp>
#include <crv/quadrature/construction/adaptive_integrator.hpp>
#include <crv/quadrature/construction/antiderivative_cache_builder.hpp>
#include <crv/quadrature/construction/bisector.hpp>
#include <crv/quadrature/construction/stack_seeder.hpp>
#include <crv/quadrature/construction/subdivider.hpp>
#include <crv/quadrature/integral.hpp>
#include <crv/quadrature/rules.hpp>
#include <crv/ranges.hpp>
#include <array>
#include <concepts>
#include <type_traits>
#include <utility>

namespace crv::quadrature {
namespace detail {

template <typename rule_t, typename integrand_t, typename scalar_t>
concept quadrature_rule_for = requires(rule_t const& rule, integrand_t const& integrand, scalar_t value) {
    typename rule_t::scalar_t;
    typename rule_t::estimate_t;
    requires std::same_as<typename rule_t::scalar_t, scalar_t>;
    { rule.estimate(value, value, integrand) } -> std::same_as<typename rule_t::estimate_t>;
    { rule.integrate(value, value, integrand) } -> std::same_as<scalar_t>;
    { rule.average(value, value, integrand) } -> std::same_as<scalar_t>;
};

} // namespace detail

/// constructs antiderivatives using the production adaptive quadrature graph
template <std::floating_point t_scalar_t> class antiderivative_factory_t
{
    using accumulator_t = compensated_accumulator_t<t_scalar_t>;
    using cache_builder_factory_t = construction::antiderivative_cache_builder_factory_t<t_scalar_t, accumulator_t>;
    using refinement_predicate_t = construction::refinement_predicate_t<t_scalar_t>;
    using bisector_t = construction::bisector_t;
    using subdivider_t = construction::subdivider_t<t_scalar_t, refinement_predicate_t, bisector_t>;
    using stack_seeder_t = construction::stack_seeder_t<t_scalar_t>;
    using integrator_t
        = construction::adaptive_integrator_t<t_scalar_t, cache_builder_factory_t, subdivider_t, stack_seeder_t>;

    static constexpr auto default_depth_limit = int_t{64};

public:
    using scalar_t = t_scalar_t;
    using default_rule_t = rules::gauss_kronrod_t<scalar_t>;
    using receipt_t = adaptive_integration_receipt_t<scalar_t>;

    template <typename integrand_t, typename rule_t = default_rule_t>
    using antiderivative_t
        = quadrature::antiderivative_t<quadrature::integral_t<std::remove_cvref_t<integrand_t>, rule_t>>;

    template <typename integrand_t, typename rule_t = default_rule_t> struct result_t
    {
        using antiderivative_t = antiderivative_factory_t::antiderivative_t<integrand_t, rule_t>;
        using receipt_t = antiderivative_factory_t::receipt_t;

        antiderivative_t antiderivative;
        receipt_t receipt;
    };

    antiderivative_factory_t() : integrator_{cache_builder_factory_t{}, subdivider_t{}, stack_seeder_t{}} {}

    template <typename integrand_t, typename rule_t = default_rule_t>
        requires detail::quadrature_rule_for<rule_t, std::remove_cvref_t<integrand_t>, scalar_t>
    [[nodiscard]] auto operator()(integrand_t integrand, scalar_t domain_end, scalar_t tolerance,
        int_t depth_limit = default_depth_limit, rule_t rule = {}) const -> result_t<integrand_t, rule_t>
    {
        return (*this)(
            std::move(integrand), domain_end, tolerance, std::array<scalar_t, 0>{}, depth_limit, std::move(rule));
    }

    template <typename integrand_t, compatible_range<scalar_t> critical_points_t, typename rule_t = default_rule_t>
        requires detail::quadrature_rule_for<rule_t, std::remove_cvref_t<integrand_t>, scalar_t>
    [[nodiscard]] auto operator()(integrand_t integrand, scalar_t domain_end, scalar_t tolerance,
        critical_points_t const& critical_points, int_t depth_limit = default_depth_limit, rule_t rule = {}) const
        -> result_t<integrand_t, rule_t>
    {
        using integral_t = quadrature::integral_t<std::remove_cvref_t<integrand_t>, rule_t>;
        auto integration = integrator_(
            integral_t{std::move(integrand), std::move(rule)}, domain_end, tolerance, depth_limit, critical_points);
        return {
            .antiderivative = std::move(integration.antiderivative),
            .receipt = integration.receipt,
        };
    }

private:
    integrator_t integrator_;
};

} // namespace crv::quadrature
