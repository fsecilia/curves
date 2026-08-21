// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <utility>
#include <vector>

namespace crv::spline {

template <typename policy_t, typename generator_factory_t> struct spline_factory_t
{
    using scalar_t = policy_t::scalar_t;
    using x_t = policy_t::x_t;
    using spline_t = policy_t::spline_t;

    generator_factory_t create_generator;

    template <typename target_t>
    void operator()(
        spline_t& spline, target_t&& target, scalar_t global_tolerance, std::vector<x_t> critical_points = {}) const
    {
        auto generate_spline = create_generator(global_tolerance);

        generate_spline(spline, std::forward<target_t>(target), std::move(critical_points));
    }
};

} // namespace crv::spline
