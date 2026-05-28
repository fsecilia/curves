// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/reflection/constraints.hpp>
#include <crv/reflection/param.hpp>

namespace crv::model::curves {

struct synchronous_t
{
    struct config_t
    {
        reflection::param_t<float_t, reflection::constraints::static_t<float_t, 1.0, 1e3>> motivity{"Motivity", 1.5};
        reflection::param_t<float_t, reflection::constraints::static_t<float_t, 1e-3, 1e3>> gamma{"Gamma", 1.0};
        reflection::param_t<float_t, reflection::constraints::static_t<float_t, 1.0 / 16, 1.0>> smooth{"Smooth", 0.5};
        reflection::param_t<float_t, reflection::constraints::static_t<float_t, 1e-3, 1e3>> sync_speed{
            "Sync Speed", 5.0};

        template <typename self_t, typename visitor_t>
        constexpr auto reflect(this self_t&& self, visitor_t&& visitor) -> decltype(auto)
        {
            self.motivity.reflect(visitor);
            self.gamma.reflect(visitor);
            self.smooth.reflect(visitor);
            self.sync_speed.reflect(visitor);

            return std::forward<visitor_t>(visitor);
        }

        constexpr auto operator==(config_t const&) const noexcept -> bool = default;
    };
};

} // namespace crv::model::curves
