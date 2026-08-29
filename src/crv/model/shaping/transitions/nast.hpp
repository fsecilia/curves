// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/jet/jet.hpp>
#include <cassert>
#include <cmath>
#include <concepts>
#include <memory>
#include <utility>

namespace crv::shaping::transitions {

/// base value and derivative of the compact C-infinity NAST transition
template <std::floating_point t_scalar_t> struct nast_base_t
{
    using scalar_t = t_scalar_t;
    using jet_t = crv::jet_t<scalar_t>;

    [[nodiscard]] auto value(scalar_t u) const noexcept -> scalar_t
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

    [[nodiscard]] auto value(jet_t u) const noexcept -> jet_t
    {
        auto const base_value = primal(u);
        return {value(base_value), derivative(base_value) * tangent(u)};
    }

    [[nodiscard]] auto derivative(scalar_t u) const noexcept -> scalar_t
    {
        if (u <= scalar_t{0} || u >= scalar_t{1}) return scalar_t{0};

        auto const base_value = value(u);
        if (base_value == scalar_t{0} || base_value == scalar_t{1}) return scalar_t{0};

        auto const complement = scalar_t{1} - u;
        auto const logit_slope = scalar_t{1} / (u * u) + scalar_t{1} / (complement * complement);
        return base_value * (scalar_t{1} - base_value) * logit_slope;
    }
};

/// compact C-infinity NAST transition backed by a retained half-domain antiderivative
template <std::floating_point t_scalar_t, typename t_antiderivative_t> class nast_t
{
public:
    using scalar_t = t_scalar_t;
    using antiderivative_t = t_antiderivative_t;
    using jet_t = crv::jet_t<scalar_t>;

    explicit nast_t(antiderivative_t antiderivative)
        : antiderivative_{std::make_shared<antiderivative_t const>(std::move(antiderivative))}
    {
        assert(antiderivative_->domain_end() == support_midpoint && "nast_t: antiderivative must cover half domain");
    }

    [[nodiscard]] auto value(scalar_t u) const noexcept -> scalar_t { return base_.value(u); }
    [[nodiscard]] auto value(jet_t u) const noexcept -> jet_t { return base_.value(u); }
    [[nodiscard]] auto derivative(scalar_t u) const noexcept -> scalar_t { return base_.derivative(u); }

    [[nodiscard]] auto antiderivative(scalar_t u) const noexcept -> scalar_t
    {
        if (u <= scalar_t{0}) return scalar_t{0};
        if (u >= scalar_t{1}) return u - support_midpoint;
        if (u <= antiderivative_->domain_end()) return (*antiderivative_)(u);
        return u - support_midpoint + (*antiderivative_)(scalar_t{1} - u);
    }

    [[nodiscard]] auto antiderivative(jet_t u) const noexcept -> jet_t
    {
        auto const base_value = primal(u);
        return {antiderivative(base_value), value(base_value) * tangent(u)};
    }

private:
    static constexpr auto support_midpoint = scalar_t{0.5};

    [[no_unique_address]] nast_base_t<scalar_t> base_;
    std::shared_ptr<antiderivative_t const> antiderivative_;
};

} // namespace crv::shaping::transitions
