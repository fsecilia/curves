// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <cassert>
#include <cmath>
#include <concepts>
#include <limits>

namespace crv::model {

/// contiguous interval of representable finite scalar inputs
template <std::floating_point scalar_t> class input_domain_t
{
public:
    constexpr input_domain_t() noexcept = default;

    constexpr input_domain_t(scalar_t first, scalar_t last) noexcept : first_{first}, last_{last}, empty_{false}
    {
        assert(std::isfinite(first) && std::isfinite(last) && "input_domain_t: endpoints must be finite");
        assert(first <= last && "input_domain_t: endpoints out of order");
    }

    /// full finite scalar interval
    [[nodiscard]] static constexpr auto full() noexcept -> input_domain_t
    {
        return {std::numeric_limits<scalar_t>::lowest(), std::numeric_limits<scalar_t>::max()};
    }

    [[nodiscard]] constexpr auto empty() const noexcept -> bool { return empty_; }

    [[nodiscard]] constexpr auto first() const noexcept -> scalar_t
    {
        assert(!empty_ && "input_domain_t: empty domain has no first input");
        return first_;
    }

    [[nodiscard]] constexpr auto last() const noexcept -> scalar_t
    {
        assert(!empty_ && "input_domain_t: empty domain has no last input");
        return last_;
    }

    [[nodiscard]] constexpr auto contains(scalar_t input) const noexcept -> bool
    {
        return !empty_ && std::isfinite(input) && first_ <= input && input <= last_;
    }

    constexpr auto operator==(input_domain_t const&) const noexcept -> bool = default;

private:
    scalar_t first_{};
    scalar_t last_{};
    bool empty_{true};
};

} // namespace crv::model
