// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>

namespace crv::pipeline {

/// elapsed time between increasing report timestamps
///
/// Non-increasing timestamps are rejected and replace the observation base; long gaps remain valid durations.
template <is_fixed t_duration_t>
    requires(!is_signed_v<t_duration_t> && t_duration_t::frac_bits == 0)
class report_timer_t
{
public:
    using duration_t = t_duration_t;
    using timestamp_t = typename duration_t::value_t;

    enum class status_t
    {
        initial,
        ready,
        invalid,
    };

    struct result_t
    {
        duration_t duration{};
        status_t status = status_t::invalid;

        constexpr auto operator==(result_t const&) const noexcept -> bool = default;
    };

    constexpr auto previous_timestamp() const noexcept -> timestamp_t { return previous_timestamp_; }
    constexpr auto initialized() const noexcept -> bool { return initialized_; }

    /// observes a timestamp and returns time since the last observation
    constexpr auto operator()(timestamp_t timestamp) noexcept -> result_t
    {
        if (!initialized_)
        {
            previous_timestamp_ = timestamp;
            initialized_ = true;
            return {.status = status_t::initial};
        }

        if (timestamp <= previous_timestamp_)
        {
            previous_timestamp_ = timestamp;
            return {.status = status_t::invalid};
        }

        auto const duration = duration_t{timestamp - previous_timestamp_};
        previous_timestamp_ = timestamp;
        return {.duration = duration, .status = status_t::ready};
    }

private:
    timestamp_t previous_timestamp_{};
    bool initialized_ = false;
};

} // namespace crv::pipeline
