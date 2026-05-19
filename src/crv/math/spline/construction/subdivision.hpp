// SPDX-License-Identifier: MIT

/// \file
/// \brief amr geometry, subdivision, and convergence
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>

namespace crv::spline {

/// result of subdividing an interval
template <typename t_interval_t> struct subdivision_t
{
    using interval_t = t_interval_t;

    interval_t left;
    interval_t right;
};

/// subdivides an interval
template <typename t_subdivision_t, typename bisector_t, typename interval_factory_t> struct subdivider_t
{
    using subdivision_t = t_subdivision_t;

    using interval_t = interval_factory_t::interval_t;
    using scalar_t = interval_t::scalar_t;

    [[no_unique_address]] bisector_t bisect;
    interval_factory_t create_interval;

    constexpr auto operator()(interval_t const& interval, auto const& sample_target_function) const noexcept
        -> subdivision_t
    {
        auto const child_domains = bisect(sample_target_function, interval.subdomain);
        return subdivision_t{
            .left = create_interval(sample_target_function, child_domains.left),
            .right = create_interval(sample_target_function, child_domains.right),
        };
    }
};

} // namespace crv::spline
