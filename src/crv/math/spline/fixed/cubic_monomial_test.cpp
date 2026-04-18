// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "cubic_monomial.hpp"
#include <crv/math/limits.hpp>
#include <crv/test/test.hpp>

namespace crv::spline::fixed_point {
namespace {

using value_t = int32_t;
using sut_t   = cubic_monomial_t<value_t>;

constexpr auto in_frac_bits  = 16;
constexpr auto out_frac_bits = 16;

using in_t  = fixed_t<value_t, in_frac_bits>;
using out_t = fixed_t<value_t, out_frac_bits>;

constexpr auto t_one  = value_t{1 << in_frac_bits};
constexpr auto t_half = value_t{1 << (in_frac_bits - 1)};

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
        return (bits < 0) ? (val >> -bits) : (val << bits);
    }
};

template <int out_frac_bits, int in_frac_bits, typename shifter_t = truncating_shifter_t>
constexpr auto evaluate(sut_t const& sut, value_t t_val, shifter_t shifter = shifter_t{}) noexcept -> value_t
{
    return sut.evaluate<out_frac_bits, in_frac_bits>(in_t::literal(t_val), shifter).value;
}

template <typename shifter_t = truncating_shifter_t>
constexpr auto evaluate(sut_t const& sut, value_t t_val, shifter_t shifter = shifter_t{}) noexcept -> value_t
{
    return evaluate<out_frac_bits, in_frac_bits, shifter_t>(sut, t_val, shifter);
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

// t = 0 == C3
static_assert(evaluate({{9999, -8888, 7777, 42}, {0, 0, 0}, out_frac_bits}, 0) == 42);

// t = 1 = C0 + C1 + C2 + C3
static_assert(evaluate({{1024, 2048, 4096, 8192}, {0, 0, 0}, out_frac_bits}, t_one) == 15360);

// extrapolation, t < 0
static_assert(evaluate({{0, 0, 1024, 0}, {0, 0, 0}, out_frac_bits}, -t_half) == -512);

// extrapolation, t > 1
static_assert(evaluate({{0, 0, 1024, 0}, {0, 0, 0}, out_frac_bits}, t_one + t_half) == 1536);

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
static_assert(evaluate({{0, 0, 0, 0}, {16, 16, 16}, out_frac_bits}, 0, perturbing_shifter_t{}) == 1);

// subbed shifter, non-zero t
static_assert(evaluate({{256, 0, 0, 0}, {0, 0, 0}, out_frac_bits}, t_half, perturbing_shifter_t{}) == 33);

// asymmetric output type
static_assert(evaluate<8, in_frac_bits>({{0, 0, 0, 16384}, {0, 0, 0}, 16}, 0) == 64);

} // namespace
} // namespace crv::spline::fixed_point
