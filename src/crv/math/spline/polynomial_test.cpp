// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "polynomial.hpp"
#include <crv/test/test.hpp>

namespace crv::spline {
namespace {

// --------------------------------------------------------------------------------------------------------------------
// fast_mac_t
// --------------------------------------------------------------------------------------------------------------------

namespace fast_mac_test {

template <typename coeff_t, typename normalized_t> struct test_t
{
    using coeff_value_t = coeff_t::value_t;
    static constexpr auto coeff_frac_bits = coeff_t::frac_bits;

    using normalized_value_t = normalized_t::value_t;
    static constexpr auto normalized_frac_bits = normalized_t::frac_bits;

    using sut_t = fast_mac_t;
    static constexpr auto sut = sut_t{};

    static constexpr auto c0 = coeff_t{0};
    static constexpr auto c1 = coeff_t{1};
    static constexpr auto cm1 = -c1; // -1.0
    static constexpr auto c1_2 = c1 >> 1; // 0.5
    static constexpr auto cm1_2 = -c1_2; // -0.5
    static constexpr auto c1_4 = c1_2 >> 1; // 0.25
    static constexpr auto cm1_4 = -c1_4; // -0.25
    static constexpr auto c1_8 = c1_4 >> 1; // 0.125
    static constexpr auto cm3_4 = cm1_2.value + cm1_4.value; // -0.75
    static constexpr auto c5 = coeff_t{5};
    static constexpr auto c_epsilon = coeff_t::literal(1);

    static constexpr auto t0 = normalized_t::literal(0);
    static constexpr auto t1_2 = normalized_t::literal(normalized_value_t{1} << (normalized_frac_bits - 1)); // 0.5
    static constexpr auto t_epsilon = normalized_t::literal(1);

    // zeros propagate correctly without shifting errors
    static_assert(sut(c0, t0, c0).value == 0);
    static_assert(sut(c1, t0, c0).value == 0);
    static_assert(sut(c0, t1_2, c0).value == 0);

    // addition of coefficient is exact when multiplication yields zero
    static_assert(sut(c0, t0, c5).value == c5.value);

    // negative rounding boundaries
    static_assert(sut(-c1_4 - c_epsilon, t1_2 - t_epsilon, c0).value == -c1_8.value);
    static_assert(sut(-c1_4 - c_epsilon, t1_2, c0).value == -c1_8.value);
    static_assert(sut(-c1_4 - c_epsilon, t1_2 + t_epsilon, c0).value == -c1_8.value - 1);
    static_assert(sut(-c1_4, t1_2 - t_epsilon, c0).value == -c1_8.value);
    static_assert(sut(-c1_4, t1_2, c0).value == -c1_8.value);
    static_assert(sut(-c1_4, t1_2 + t_epsilon, c0).value == -c1_8.value);
    static_assert(sut(-c1_4 + c_epsilon, t1_2 - t_epsilon, c0).value == -c1_8.value + 1);
    static_assert(sut(-c1_4 + c_epsilon, t1_2, c0).value == -c1_8.value + 1);
    static_assert(sut(-c1_4 + c_epsilon, t1_2 + t_epsilon, c0).value == -c1_8.value);

    // positive rounding boundaries
    static_assert(sut(c1_4 - c_epsilon, t1_2 - t_epsilon, c0).value == c1_8.value - 1);
    static_assert(sut(c1_4 - c_epsilon, t1_2, c0).value == c1_8.value);
    static_assert(sut(c1_4 - c_epsilon, t1_2 + t_epsilon, c0).value == c1_8.value);
    static_assert(sut(c1_4, t1_2 - t_epsilon, c0).value == c1_8.value);
    static_assert(sut(c1_4, t1_2, c0).value == c1_8.value);
    static_assert(sut(c1_4, t1_2 + t_epsilon, c0).value == c1_8.value);
    static_assert(sut(c1_4 + c_epsilon, t1_2 - t_epsilon, c0).value == c1_8.value);
    static_assert(sut(c1_4 + c_epsilon, t1_2, c0).value == c1_8.value + 1);
    static_assert(sut(c1_4 + c_epsilon, t1_2 + t_epsilon, c0).value == c1_8.value + 1);

    // sign
    static_assert(sut(cm1, t1_2, c0).value == cm1_2.value);
    static_assert(sut(cm1, t1_2, cm1_4).value == cm3_4);

    // limits
    static_assert(sut(min<coeff_t>(), t0, min<coeff_t>()).value == min<coeff_t>().value);
    static_assert(sut(max<coeff_t>(), t0, min<coeff_t>()).value == min<coeff_t>().value);
    static_assert(sut(min<coeff_t>(), t0, c0).value == 0);
    static_assert(sut(max<coeff_t>(), t0, c0).value == 0);
    static_assert(sut(min<coeff_t>(), t0, max<coeff_t>()).value == max<coeff_t>().value);
    static_assert(sut(max<coeff_t>(), t0, max<coeff_t>()).value == max<coeff_t>().value);

    // to test against t_max through rounding we need headroom; |acc| must be less than 2^(normalized_frac_bits - 1)
    static constexpr auto bound_value = coeff_value_t{1} << (normalized_frac_bits - 2);
    static constexpr auto max_bound_c = coeff_t::literal(bound_value);
    static constexpr auto min_bound_c = coeff_t::literal(-bound_value);
    static_assert(sut(min_bound_c, max<normalized_t>(), c0).value == min_bound_c.value);
    static_assert(sut(max_bound_c, max<normalized_t>(), c0).value == max_bound_c.value);
};

template struct test_t<fixed_t<int64_t, 48>, fixed_t<uint64_t, 64>>;
template struct test_t<fixed_t<int64_t, 12>, fixed_t<uint32_t, 32>>;
template struct test_t<fixed_t<int32_t, 11>, fixed_t<uint16_t, 16>>;

} // namespace fast_mac_test

} // namespace
} // namespace crv::spline
