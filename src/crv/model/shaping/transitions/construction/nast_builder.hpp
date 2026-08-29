// SPDX-License-Identifier: MIT

/// \file
/// \brief constructs a NAST transition and validates its adaptive quadrature receipt
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/model/shaping/transitions/nast.hpp>
#include <crv/quadrature/construction/adaptive_integrator.hpp>
#include <array>
#include <cmath>
#include <expected>
#include <utility>

namespace crv::shaping::transitions::construction {

template <typename t_integrator_t = quadrature::construction::adaptive_integrator_t<float_t>,
    typename t_transition_t = nast_t>
class nast_builder_t
{
public:
    using integrator_t = t_integrator_t;
    using transition_t = t_transition_t;
    using scalar_t = float_t;
    using integral_t = transitions::detail::nast_integral_t;

    struct receipt_t
    {
        scalar_t requested_tolerance;
        scalar_t achieved_error;
        scalar_t max_error;
        int_t segment_count;
        bool refinement_limited;

        constexpr auto operator==(receipt_t const&) const noexcept -> bool = default;
    };

    struct error_t
    {
        scalar_t requested_tolerance;
        scalar_t achieved_error;
        scalar_t max_error;
        bool refinement_limited;

        constexpr auto operator==(error_t const&) const noexcept -> bool = default;
    };

    struct product_t
    {
        transition_t transition;
        receipt_t receipt;
    };

    using result_t = std::expected<product_t, error_t>;

    static constexpr auto domain_end = scalar_t{0.5};
    static constexpr auto requested_tolerance = scalar_t{1e-12};
    static constexpr auto depth_limit = int_t{32};

    constexpr explicit nast_builder_t(integrator_t integrator) noexcept : integrator_{std::move(integrator)} {}

    [[nodiscard]] auto operator()() const -> result_t
    {
        auto integration = integrator_(
            integral_t{transitions::detail::nast_integrand_t{}, transitions::detail::nast_rule_t{}}, domain_end,
            std::array<scalar_t, 0>{});
        auto const error = error_t{
            .requested_tolerance = requested_tolerance,
            .achieved_error = integration.achieved_error,
            .max_error = integration.max_error,
            .refinement_limited = integration.refinement_limited,
        };

        if (integration.refinement_limited || !std::isfinite(integration.achieved_error)
            || !std::isfinite(integration.max_error) || integration.achieved_error < scalar_t{0}
            || integration.max_error < scalar_t{0} || integration.achieved_error > requested_tolerance)
            return std::unexpected{error};

        auto const receipt = receipt_t{
            .requested_tolerance = requested_tolerance,
            .achieved_error = integration.achieved_error,
            .max_error = integration.max_error,
            .segment_count = integration.antiderivative.segment_count(),
            .refinement_limited = integration.refinement_limited,
        };
        return product_t{.transition = transition_t{std::move(integration.antiderivative)}, .receipt = receipt};
    }

private:
    [[no_unique_address]] integrator_t integrator_;
};

} // namespace crv::shaping::transitions::construction
