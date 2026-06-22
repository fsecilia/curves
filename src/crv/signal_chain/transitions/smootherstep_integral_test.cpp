// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "smootherstep_integral.hpp"
#include <crv/test/test.hpp>

namespace crv::signal_chain::transitions {
namespace {

constexpr smootherstep_integral_t sut{};

template <typename scalar_t> struct test_t
{
    //
    // scalars
    //

    // below zero clamps to 0
    static_assert(sut(scalar_t{-1.5}) == scalar_t{0.0});

    // exactly zero
    static_assert(sut(scalar_t{0.0}) == scalar_t{0.0});

    // interior midpoint
    static_assert(sut(scalar_t{0.5}) == scalar_t{0.078125});

    // exactly one evaluates to (1 - 0.5)
    static_assert(sut(scalar_t{1.0}) == scalar_t{0.5});

    // above one evaluates to (t - 0.5)
    static_assert(sut(scalar_t{2.0}) == scalar_t{1.5});

    //
    // jets
    //

    using jet_t = jet_t<scalar_t>;

    // below zero: derivative should be 0
    static_assert(sut(jet_t{scalar_t{-1.0}, scalar_t{1.0}}) == jet_t{scalar_t{0.0}, scalar_t{0.0}});

    // interior midpoint
    static_assert(sut(jet_t{scalar_t{0.5}, scalar_t{1.0}}) == jet_t{scalar_t{0.078125}, scalar_t{0.5}});

    // chain rule, dy_dt * dt
    static_assert(sut(jet_t{scalar_t{0.5}, scalar_t{2.0}}) == jet_t{scalar_t{0.078125}, scalar_t{1.0}});

    // above one: linear slope, derivative is strictly 1.0 * dt
    static_assert(sut(jet_t{scalar_t{2.0}, scalar_t{3.0}}) == jet_t{scalar_t{1.5}, scalar_t{3.0}});
};

template struct test_t<float32_t>;
template struct test_t<float64_t>;

} // namespace
} // namespace crv::signal_chain::transitions
