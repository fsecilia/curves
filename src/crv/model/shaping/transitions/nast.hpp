// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/jet/jet.hpp>
#include <crv/quadrature/adaptive_integrator.hpp>
#include <crv/quadrature/integral.hpp>
#include <crv/quadrature/rules.hpp>
#include <array>
#include <cmath>
#include <expected>
#include <utility>

namespace crv::shaping::transitions {

/// normalized NAST antiderivative-cache policy
struct nast_cache_config_t
{
    static constexpr auto domain_end = float_t{0.5};
    static constexpr auto requested_tolerance = float_t{1e-12};
    static constexpr auto depth_limit = int_t{32};
};

struct nast_cache_receipt_t
{
    float_t requested_tolerance;
    float_t achieved_error;
    float_t max_error;
    int_t segment_count;
    bool refinement_limited;

    constexpr auto operator==(nast_cache_receipt_t const&) const noexcept -> bool = default;
};

struct nast_cache_error_t
{
    float_t requested_tolerance;
    float_t achieved_error;
    float_t max_error;
    bool refinement_limited;

    constexpr auto operator==(nast_cache_error_t const&) const noexcept -> bool = default;
};

namespace detail {

/// normalized sigmoid form of the non-analytic smooth transition
struct nast_integrand_t
{
    using scalar_t = float_t;

    [[nodiscard]] auto operator()(scalar_t u) const noexcept -> scalar_t
    {
        if (u <= scalar_t{0}) return scalar_t{0};
        if (u >= scalar_t{1}) return scalar_t{1};

        auto const log_b_over_a = scalar_t{1} / u - scalar_t{1} / (scalar_t{1} - u);
        if (log_b_over_a >= scalar_t{0})
        {
            auto const exp_neg = std::exp(-log_b_over_a);
            return exp_neg / (scalar_t{1} + exp_neg);
        }

        auto const exp_pos = std::exp(log_b_over_a);
        return scalar_t{1} / (scalar_t{1} + exp_pos);
    }
};

struct nast_integrator_t
{
    using scalar_t = float_t;
    using rule_t = quadrature::rules::gauss_kronrod_t<scalar_t>;
    using integral_t = quadrature::integral_t<nast_integrand_t, rule_t>;
    using result_t = quadrature::integration_result_of_t<integral_t>;

    [[nodiscard]] auto operator()(scalar_t tolerance, int_t depth_limit) const -> result_t
    {
        auto integrate = quadrature::adaptive_integrator_t<scalar_t>{tolerance, depth_limit};
        return integrate(integral_t{nast_integrand_t{}, rule_t{}}, nast_cache_config_t::domain_end,
            std::array<scalar_t, 0>{});
    }
};

} // namespace detail

template <typename antiderivative_t> struct nast_cache_t
{
    antiderivative_t antiderivative;
    nast_cache_receipt_t receipt;
};

namespace generic {

/// validates one adaptive NAST integration and turns it into an immutable cache
///
/// The integrator seam keeps numerical policy tests independent from adaptive quadrature itself.
template <typename t_integrator_t> class nast_cache_builder_t
{
public:
    using integrator_t = t_integrator_t;
    using integration_result_t = typename integrator_t::result_t;
    using scalar_t = typename integration_result_t::scalar_t;
    using antiderivative_t = typename integration_result_t::antiderivative_t;
    using cache_t = nast_cache_t<antiderivative_t>;
    using result_t = std::expected<cache_t, nast_cache_error_t>;

    constexpr explicit nast_cache_builder_t(integrator_t integrator = {}) noexcept
        : integrator_{std::move(integrator)}
    {}

    [[nodiscard]] auto operator()() const -> result_t
    {
        auto integration = integrator_(nast_cache_config_t::requested_tolerance, nast_cache_config_t::depth_limit);
        auto const error = nast_cache_error_t{
            .requested_tolerance = nast_cache_config_t::requested_tolerance,
            .achieved_error = integration.achieved_error,
            .max_error = integration.max_error,
            .refinement_limited = integration.refinement_limited,
        };

        if (integration.refinement_limited || !std::isfinite(integration.achieved_error)
            || !std::isfinite(integration.max_error) || integration.achieved_error < scalar_t{0}
            || integration.max_error < scalar_t{0}
            || integration.achieved_error > nast_cache_config_t::requested_tolerance)
            return std::unexpected{error};

        auto const receipt = nast_cache_receipt_t{
            .requested_tolerance = nast_cache_config_t::requested_tolerance,
            .achieved_error = integration.achieved_error,
            .max_error = integration.max_error,
            .segment_count = integration.antiderivative.segment_count(),
            .refinement_limited = integration.refinement_limited,
        };
        return cache_t{.antiderivative = std::move(integration.antiderivative), .receipt = receipt};
    }

private:
    [[no_unique_address]] integrator_t integrator_;
};

} // namespace generic

/// compact C-infinity NAST transition backed by one process-lifetime antiderivative cache
class nast_t
{
private:
    using cache_builder_t = generic::nast_cache_builder_t<detail::nast_integrator_t>;
    using cache_t = typename cache_builder_t::cache_t;

public:
    using construction_error_t = nast_cache_error_t;
    using construction_result_t = std::expected<nast_t, construction_error_t>;
    using scalar_t = float_t;
    using jet_t = crv::jet_t<scalar_t>;

    [[nodiscard]] static auto make() -> construction_result_t
    {
        static auto const cache = cache_builder_t{}();
        if (!cache) return std::unexpected{cache.error()};
        return nast_t{*cache};
    }

    [[nodiscard]] auto operator()(scalar_t u) const noexcept -> scalar_t { return detail::nast_integrand_t{}(u); }

    [[nodiscard]] auto operator()(jet_t u) const noexcept -> jet_t
    {
        auto const value = primal(u);
        return {operator()(value), derivative(value) * tangent(u)};
    }

    [[nodiscard]] auto derivative(scalar_t u) const noexcept -> scalar_t
    {
        if (u <= scalar_t{0} || u >= scalar_t{1}) return scalar_t{0};

        auto const value = operator()(u);
        if (value == scalar_t{0} || value == scalar_t{1}) return scalar_t{0};

        auto const complement = scalar_t{1} - u;
        auto const logit_slope
            = scalar_t{1} / (u * u) + scalar_t{1} / (complement * complement);
        return value * (scalar_t{1} - value) * logit_slope;
    }

    [[nodiscard]] auto antiderivative(scalar_t u) const noexcept -> scalar_t
    {
        if (u <= scalar_t{0}) return scalar_t{0};
        if (u >= scalar_t{1}) return u - scalar_t{0.5};
        if (u <= nast_cache_config_t::domain_end) return cache_->antiderivative(u);
        return u - scalar_t{0.5} + cache_->antiderivative(scalar_t{1} - u);
    }

    [[nodiscard]] auto antiderivative(jet_t u) const noexcept -> jet_t
    {
        auto const value = primal(u);
        return {antiderivative(value), operator()(value) * tangent(u)};
    }

    [[nodiscard]] auto construction_receipt() const noexcept -> nast_cache_receipt_t const& { return cache_->receipt; }

private:
    explicit nast_t(cache_t const& cache) noexcept : cache_{&cache} {}

    cache_t const* cache_;
};

} // namespace crv::shaping::transitions
