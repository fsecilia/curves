// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "hermite_converter.hpp"
#include <crv/test/test.hpp>
#include <array>

namespace crv::spline {
namespace {

using scalar_t = float_t;
using jet_t = jet_t<scalar_t>;
using cubic_t = cubic_t<scalar_t>;
using sut_t = hermite_converter_t<scalar_t>;

constexpr auto sut = sut_t{};

// zero baseline
static_assert(cubic_t{0, 0, 0, 0} == sut(jet_t{0.0, 0.0}, jet_t{0.0, 0.0}));

//
// single-input isolation
//

// p0 = 1.0 (dp = -1.0)
// a = -2(-1) = 2, b = 3(-1) = -3, c = 0, d = 1
static_assert(cubic_t{2.0, -3.0, 0.0, 1.0} == sut(jet_t{1.0, 0.0}, jet_t{0.0, 0.0}));

// p1 = 1.0 (dp = 1.0)
// a = -2(1) = -2, b = 3(1) = 3, c = 0, d = 0
static_assert(cubic_t{-2.0, 3.0, 0, 0} == sut(jet_t{0.0, 0.0}, jet_t{1.0, 0.0}));

// m0 = 1.0
// a = 1, b = -2, c = 1, d = 0
static_assert(cubic_t{1.0, -2.0, 1.0, 0} == sut(jet_t{0.0, 1.0}, jet_t{0.0, 0.0}));

// m1 = 1.0
// a = 1, b = -1, c = 0, d = 0
static_assert(cubic_t{1.0, -1.0, 0, 0} == sut(jet_t{0.0, 0.0}, jet_t{0.0, 1.0}));

//
// combined inputs
//

// dp = 0, tangent-only: p0 = p1 = 1.0, m0 = 1.0, m1 = -1.0
// a = 0 + 1 + (-1) = 0, b = 0 - 2 - (-1) = -1, c = 1, d = 1
static_assert(cubic_t{0, -1.0, 1.0, 1.0} == sut(jet_t{1.0, 1.0}, jet_t{1.0, -1.0}));

// negative dp with all inputs active: p0 = 2, m0 = 0.5, p1 = -1, m1 = -0.5 (dp = -3)
// a = 6 + 0.5 + (-0.5) = 6, b = -9 - 1 - (-0.5) = -9.5, c = 0.5, d = 2
static_assert(cubic_t{6.0, -9.5, 0.5, 2.0} == sut(jet_t{2.0, 0.5}, jet_t{-1.0, -0.5}));

//
// linear inputs (cubic and quadratic vanish)
//

// y = x: p0 = 0, m0 = 1, p1 = 1, m1 = 1
static_assert(cubic_t{0, 0, 1.0, 0} == sut(jet_t{0.0, 1.0}, jet_t{1.0, 1.0}));

// y = 2x + 1: p0 = 1, m0 = 2, p1 = 3, m1 = 2
static_assert(cubic_t{0, 0, 2.0, 1.0} == sut(jet_t{1.0, 2.0}, jet_t{3.0, 2.0}));

// mixed fractional and negative
//
// p0 = 0.5, m0 = -0.5, p1 = -0.5, m1 = 0.5 (dp = -1)
// a = 2 + (-0.5) + 0.5 = 2, b = -3 + 1 - 0.5 = -2.5, c = -0.5, d = 0.5
static_assert(cubic_t{2.0, -2.5, -0.5, 0.5} == sut(jet_t{0.5, -0.5}, jet_t{-0.5, 0.5}));

// large magnitude
//
// p0 = -50, m0 = 10, p1 = 50, m1 = -10 (dp = 100)
// a = -200 + 10 + (-10) = -200, b = 300 - 20 + 10 = 290, c = 10, d = -50
static_assert(cubic_t{-200.0, 290.0, 10.0, -50.0} == sut(jet_t{-50.0, 10.0}, jet_t{50.0, -10.0}));

} // namespace
} // namespace crv::spline
