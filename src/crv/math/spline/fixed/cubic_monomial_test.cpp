// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "cubic_monomial.hpp"
#include <crv/math/limits.hpp>
#include <crv/test/test.hpp>

namespace crv::spline::fixed_point {
namespace {

using value_t = int64_t;
using sut_t   = cubic_monomial_t<value_t>;

constexpr auto normalized_frac_bits = sut_t::normalized_frac_bits;
using normalized_t                  = sut_t::normalized_t;
using normalized_value_t            = normalized_t::value_t;

constexpr auto out_frac_bits = 16;
using out_t                  = fixed_t<value_t, out_frac_bits>;

constexpr auto t_half          = normalized_value_t{1ULL << (normalized_frac_bits - 1)};
constexpr auto t_quarter       = t_half >> 1;
constexpr auto t_three_quarter = t_half + t_quarter;
constexpr auto t_messy         = 0xAAAAAAAAAAAAAAAAULL; // roughly 2/3

struct truncating_shifter_t
{
    template <typename val_t> [[nodiscard]] constexpr auto shr(val_t val, int_t bits) const noexcept -> val_t
    {
        return val >> bits;
    }

    template <typename val_t> [[nodiscard]] constexpr auto shift(val_t val, int_t bits) const noexcept -> val_t
    {
        return (bits < 0) ? (val >> -bits) : (val << bits);
    }
};

struct perturbing_shifter_t
{
    template <typename val_t> [[nodiscard]] constexpr auto shr(val_t val, int_t bits) const noexcept -> val_t
    {
        return (val >> bits) + 1;
    }

    template <typename val_t> [[nodiscard]] constexpr auto shift(val_t val, int_t bits) const noexcept -> val_t
    {
        return ((bits < 0) ? (val >> -bits) : (val << bits)) + 10;
    }
};

template <int out_frac_bits, typename shifter_t = truncating_shifter_t>
constexpr auto evaluate(cubic_monomial_t<value_t, shifter_t> const& sut, normalized_value_t t_val) noexcept -> value_t
{
    return sut.template evaluate<out_frac_bits>(normalized_t::literal(t_val)).value;
}

template <typename shifter_t = truncating_shifter_t>
constexpr auto evaluate(cubic_monomial_t<value_t, shifter_t> const& sut, normalized_value_t t_val) noexcept -> value_t
{
    return evaluate<out_frac_bits>(sut, t_val);
}

// all zeros
static_assert(evaluate({{0, 0, 0, 0}, {0, 0, 0}, out_frac_bits}, t_half) == 0);

// early coeff
static_assert(evaluate({{32, 0, 0, 0}, {0, 0, 0}, out_frac_bits}, t_half) == 4);

// late coeff
static_assert(evaluate({{0, 0, 0, 1024}, {0, 0, 0}, out_frac_bits}, t_half) == 1024);

// all coeffs
static_assert(evaluate({{1024, 2048, 4096, 8192}, {0, 0, 0}, out_frac_bits}, t_half) == 10880);

// negative early coeff
static_assert(evaluate({{-16, 0, 0, 0}, {0, 0, 0}, out_frac_bits}, t_half) == -2);

// negative late coeff
static_assert(evaluate({{0, 0, 0, -100}, {0, 0, 0}, out_frac_bits}, t_half) == -100);

// all negative coeffs
static_assert(evaluate({{-1024, -2048, -4096, -8192}, {0, 0, 0}, out_frac_bits}, t_half) == -10880);

// t = 0 -> C3
static_assert(evaluate({{9999, -8888, 7777, 42}, {0, 0, 0}, out_frac_bits}, 0) == 42);

// t = 1/4
static_assert(evaluate({{1024, 1024, 1024, 1024}, {0, 0, 0}, out_frac_bits}, t_quarter) == 1360);

// t = 0xAAAA... (~2/3), isolates truncation behavior on non-power-of-two fractions
static_assert(evaluate({{0, 0, 81, 0}, {0, 0, 0}, out_frac_bits}, t_messy) == 53);

// t = 3/4
static_assert(evaluate({{256, 256, 256, 256}, {0, 0, 0}, out_frac_bits}, t_three_quarter) == 700);

// t = max -> C0(1 - epsilon) + C1(1 - epsilon) + C2(1 - epsilon) + C3 = C0 + C1 + C2 + C3 - 3
static_assert(evaluate({{1024, 2048, 4096, 8192}, {0, 0, 0}, out_frac_bits}, max<normalized_value_t>()) == 15357);

// relative shift
static_assert(evaluate({{2, 4, 0, 0}, {16, 0, 0}, out_frac_bits}, t_half) == 1);

// negative relative shift
static_assert(evaluate({{1024, 0, 0, 0}, {-4, 0, 0}, out_frac_bits}, t_half) == 2048);

// mixed shifts
static_assert(evaluate({{1024, 2048, 4096, 8192}, {2, -1, 4}, out_frac_bits}, t_half) == 8388);

// downscale
static_assert(evaluate({{0, 0, 0, 4096}, {0, 0, 0}, 18}, 0) == 1024);

// upscale
static_assert(evaluate({{0, 0, 0, 256}, {0, 0, 0}, 14}, 0) == 1024);

// subbed shifter, t = 0
static_assert(evaluate<perturbing_shifter_t>({{0, 0, 0, 0}, {16, 16, 16}, out_frac_bits}, 0) == 11);

// subbed shifter, non-zero t
static_assert(evaluate<perturbing_shifter_t>({{256, 0, 0, 0}, {0, 0, 0}, out_frac_bits}, t_half) == 43);

// asymmetric output type
static_assert(evaluate<8>({{0, 0, 0, 16384}, {0, 0, 0}, 16}, 0) == 64);

} // namespace
} // namespace crv::spline::fixed_point
