// SPDX-License-Identifier: MIT

/// \file
/// \brief constructs the retained transition inventory
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/model/shaping/transitions/factory.hpp>
#include <crv/model/shaping/transitions/nast.hpp>
#include <array>
#include <cassert>
#include <cmath>
#include <utility>

namespace crv::shaping::transitions::construction {

template <typename t_antiderivative_factory_t> class transition_factory_builder_t
{
    using antiderivative_factory_t = t_antiderivative_factory_t;
    using scalar_t = antiderivative_factory_t::scalar_t;

    struct integrand_t
    {
        [[nodiscard]] auto operator()(scalar_t u) const noexcept -> scalar_t { return base.value(u); }

        [[no_unique_address]] transitions::nast_base_t<scalar_t> base;
    };

    using critical_points_t = std::array<scalar_t, 0>;
    using antiderivative_t = antiderivative_factory_t::template antiderivative_t<integrand_t>;
    using quadrature_receipt_t = antiderivative_factory_t::receipt_t;
    using nast_t = transitions::nast_t<scalar_t, antiderivative_t>;

    static constexpr auto nast_domain_end = scalar_t{0.5};

public:
    using factory_t = transitions::transition_factory_t<nast_t, quadrature_receipt_t>;

    constexpr transition_factory_builder_t(
        antiderivative_factory_t build_antiderivative, scalar_t tolerance, int_t depth_limit) noexcept
        : build_antiderivative_{std::move(build_antiderivative)}, tolerance_{tolerance}, depth_limit_{depth_limit}
    {}

    [[nodiscard]] auto operator()() const -> factory_t
    {
        auto integration
            = build_antiderivative_(integrand_t{}, nast_domain_end, tolerance_, critical_points_t{}, depth_limit_);
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
    [[no_unique_address]] antiderivative_factory_t build_antiderivative_;
    scalar_t tolerance_;
    int_t depth_limit_;
};

} // namespace crv::shaping::transitions::construction
