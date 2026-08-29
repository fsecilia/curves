// SPDX-License-Identifier: MIT

/// \file
/// \brief selects concrete transitions from the configured transition inventory
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/model/shaping/transitions/continuity.hpp>
#include <crv/model/shaping/transitions/smootherstep.hpp>
#include <crv/model/shaping/transitions/smootheststep.hpp>
#include <crv/model/shaping/transitions/smoothstep.hpp>
#include <cassert>
#include <optional>
#include <utility>

namespace crv::shaping::transitions {

template <typename t_transition_t, typename t_quadrature_receipt_t> struct transition_product_t
{
    using transition_t = t_transition_t;
    using quadrature_receipt_t = t_quadrature_receipt_t;

    transition_t transition;
    std::optional<quadrature_receipt_t> quadrature_receipt;
};

template <typename t_nast_t, typename t_quadrature_receipt_t> class transition_factory_t
{
public:
    using nast_t = t_nast_t;
    using quadrature_receipt_t = t_quadrature_receipt_t;
    using nast_product_t = transition_product_t<nast_t, quadrature_receipt_t>;

    explicit transition_factory_t(nast_product_t nast) noexcept : nast_{std::move(nast)} {}

    template <typename continuation_t>
    [[nodiscard]] auto operator()(continuity_t continuity, continuation_t&& continuation) const -> decltype(auto)
    {
        switch (continuity)
        {
            case continuity_t::c1:
                return std::forward<continuation_t>(continuation)(
                    transition_product_t<smoothstep_t, quadrature_receipt_t>{});
            case continuity_t::c2:
                return std::forward<continuation_t>(continuation)(
                    transition_product_t<smootherstep_t, quadrature_receipt_t>{});
            case continuity_t::c3:
                return std::forward<continuation_t>(continuation)(
                    transition_product_t<smootheststep_t, quadrature_receipt_t>{});
            case continuity_t::cinfinity: return std::forward<continuation_t>(continuation)(nast_product_t{nast_});
        }

        assert(false && "transition_factory_t: continuity out of range");
        std::unreachable();
    }

private:
    nast_product_t nast_;
};

} // namespace crv::shaping::transitions
