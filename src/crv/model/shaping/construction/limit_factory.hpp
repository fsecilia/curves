// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <utility>

namespace crv::shaping::construction {

template <typename real_t, typename limit_t, typename transition_t, typename locator_t, typename engager_t>
class limit_factory_t
{
public:
    constexpr limit_factory_t(locator_t locate, engager_t engage) noexcept
        : locator_{std::move(locate)}, engage_{std::move(engage)}
    {}

    template <typename curve_t>
    [[nodiscard]] constexpr auto operator()(real_t input_min, real_t input_max, real_t transition_width,
        real_t transition_height, real_t min_transition_width, real_t output_limit, transition_t transition,
        curve_t const& curve) const noexcept -> limit_t
    {
        auto const transition_max = transition(real_t{1});
        auto const transition_start
            = locator_(input_min, input_max, transition_width, transition_max, output_limit, curve);

        auto const output_max = curve(input_max);
        auto const blend = engage_(output_max, output_limit, transition_height);

        return limit_t{transition_start, transition_width, min_transition_width, blend, std::move(transition)};
    }

private:
    locator_t locator_;
    engager_t engage_;
};

} // namespace crv::shaping::construction
