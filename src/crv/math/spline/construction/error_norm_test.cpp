// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "error_norm.hpp"
#include <crv/test/test.hpp>

namespace crv::error_norms {
namespace {

struct jet_t
{
    using value_t = float_t;

    value_t value;
    value_t derivative;

    friend constexpr auto primal(jet_t j) noexcept -> value_t { return j.value; }
    friend constexpr auto derivative(jet_t j) noexcept -> value_t { return j.derivative; }
};

// ====================================================================================================================
// uniform_t
// ====================================================================================================================

constexpr auto scalar_norm = uniform_t<double>{};

static_assert(abs(scalar_norm(5.0, 5.0) - 0.0) < 1e-9);
static_assert(abs(scalar_norm(5.0, 3.0) - 2.0) < 1e-9);
static_assert(abs(scalar_norm(3.0, 5.0) - 2.0) < 1e-9);

// ====================================================================================================================
// first_order_uniform_t
// ====================================================================================================================

// --------------------------------------------------------------------------------------------------------------------
// Unweighted
// --------------------------------------------------------------------------------------------------------------------

constexpr auto jet_norm_default_weight = first_order_uniform_t<jet_t>{};

constexpr auto target_jet = jet_t{10.0, 1.0};
constexpr auto exact_approx = jet_t{10.0, 1.0};
constexpr auto pos_error_approx = jet_t{8.0, 1.0}; // position error = 2, tangent = 0
constexpr auto tan_error_approx = jet_t{10.0, -2.0}; // position error = 0, tangent = 3
constexpr auto mixed_error_approx = jet_t{6.0, 0.0}; // position error = 4, tangent = 1

static_assert(abs(jet_norm_default_weight(target_jet, exact_approx) - 0.0) < 1e-9);
static_assert(abs(jet_norm_default_weight(target_jet, pos_error_approx) - 2.0) < 1e-9);
static_assert(abs(jet_norm_default_weight(target_jet, tan_error_approx) - 3.0) < 1e-9);
static_assert(abs(jet_norm_default_weight(target_jet, mixed_error_approx) - 4.0) < 1e-9);

// --------------------------------------------------------------------------------------------------------------------
// Weighted
// --------------------------------------------------------------------------------------------------------------------

constexpr auto jet_norm_custom_weight = first_order_uniform_t<jet_t>{0.5};

static_assert(abs(jet_norm_custom_weight(target_jet, tan_error_approx) - 1.5) < 1e-9);
static_assert(abs(jet_norm_custom_weight(target_jet, mixed_error_approx) - 4.0) < 1e-9);

} // namespace
} // namespace crv::error_norms
