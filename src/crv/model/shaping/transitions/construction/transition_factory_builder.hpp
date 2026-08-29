// SPDX-License-Identifier: MIT

/// \file
/// \brief constructs the retained transition inventory
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/model/shaping/transitions/factory.hpp>
#include <crv/model/shaping/transitions/nast.hpp>
#include <crv/quadrature/integral.hpp>
#include <crv/quadrature/rules.hpp>
#include <array>
#include <cassert>
#include <cmath>
#include <concepts>
#include <type_traits>
#include <utility>

namespace crv::shaping::transitions::construction {

template <std::floating_point t_scalar_t, typename t_integrator_t> class transition_factory_builder_t
{
public:
    using scalar_t = t_scalar_t;
    using integrator_t = t_integrator_t;

    struct integrand_t
    {
        [[nodiscard]] auto operator()(scalar_t u) const noexcept -> scalar_t { return base.value(u); }

        [[no_unique_address]] transitions::nast_base_t<scalar_t> base;
    };

    using rule_t = quadrature::rules::gauss_kronrod_t<scalar_t>;
    using integral_t = quadrature::integral_t<integrand_t, rule_t>;
    using critical_points_t = std::array<scalar_t, 0>;
    using integration_result_t = std::remove_cvref_t<decltype(std::declval<integrator_t const&>()(
        std::declval<integral_t>(), std::declval<scalar_t>(), std::declval<critical_points_t const&>()))>;
    using antiderivative_t = typename integration_result_t::antiderivative_t;
    using quadrature_receipt_t = typename integration_result_t::receipt_t;
    using nast_t = transitions::nast_t<scalar_t, antiderivative_t>;
    using factory_t = transitions::transition_factory_t<nast_t, quadrature_receipt_t>;

    static constexpr auto nast_domain_end = scalar_t{0.5};

    constexpr explicit transition_factory_builder_t(integrator_t integrator) noexcept
        : integrator_{std::move(integrator)}
    {}

    [[nodiscard]] auto operator()() const -> factory_t
    {
        auto integration = integrator_(integral_t{integrand_t{}, rule_t{}}, nast_domain_end, critical_points_t{});
        auto const& receipt = integration.receipt;

        assert(std::isfinite(receipt.requested_tolerance) && receipt.requested_tolerance > scalar_t{0}
            && "transition_factory_builder_t: quadrature tolerance must be finite and positive");
        assert(!receipt.refinement_limited && "transition_factory_builder_t: NAST quadrature refinement limited");
        assert(std::isfinite(receipt.achieved_error) && receipt.achieved_error >= scalar_t{0}
            && "transition_factory_builder_t: NAST achieved error must be finite and nonnegative");
        assert(std::isfinite(receipt.max_error) && receipt.max_error >= scalar_t{0}
            && "transition_factory_builder_t: NAST max error must be finite and nonnegative");
        assert(receipt.achieved_error <= receipt.requested_tolerance
            && "transition_factory_builder_t: NAST quadrature missed requested tolerance");

        auto product = transitions::transition_product_t<nast_t, quadrature_receipt_t>{
            .transition = nast_t{std::move(integration.antiderivative)},
            .quadrature_receipt = receipt,
        };
        return factory_t{std::move(product)};
    }

private:
    [[no_unique_address]] integrator_t integrator_;
};

} // namespace crv::shaping::transitions::construction
