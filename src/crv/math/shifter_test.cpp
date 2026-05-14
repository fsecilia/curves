// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "shifter.hpp"
#include <crv/test/test.hpp>
#include <gmock/gmock.h>

namespace crv {

// ====================================================================================================================
// fixtures
// ====================================================================================================================

template <typename t_underlying_t, typename tag_t> struct strong_type_t
{
    using underlying_t = t_underlying_t;
    underlying_t underlying;

    explicit constexpr strong_type_t(underlying_t underlying = {}) noexcept : underlying{underlying} {}

    template <typename other_underlying_t, typename other_tag_t>
    explicit constexpr strong_type_t(strong_type_t<other_underlying_t, other_tag_t> other) noexcept
        : underlying{static_cast<underlying_t>(other.underlying)}
    {}

    constexpr auto operator>>(int_t count) const noexcept -> strong_type_t
    {
        return strong_type_t{underlying >> count};
    }

    constexpr auto operator<<(int_t count) const noexcept -> strong_type_t
    {
        return strong_type_t{underlying << count};
    }

    constexpr auto operator<=>(strong_type_t const& other) const noexcept -> auto = default;
    constexpr auto operator==(strong_type_t const& other) const noexcept -> bool = default;
};

template <typename underlying_t, typename tag_t> struct min_max_t<strong_type_t<underlying_t, tag_t>>
{
    static constexpr auto min = strong_type_t<underlying_t, tag_t>{crv::min<underlying_t>()};
    static constexpr auto max = strong_type_t<underlying_t, tag_t>{crv::max<underlying_t>()};
};

} // namespace crv

namespace std {

template <typename underlying_t, typename tag_t>
struct numeric_limits<crv::strong_type_t<underlying_t, tag_t>> : numeric_limits<underlying_t>
{
    static constexpr bool is_specialized = true;

    using src_t = numeric_limits<underlying_t>;
    using dst_t = crv::strong_type_t<underlying_t, tag_t>;

    static constexpr auto max() noexcept -> dst_t { return dst_t{src_t::max()}; }
    static constexpr auto lowest() noexcept -> dst_t { return dst_t{src_t::lowest()}; }
};

} // namespace std

namespace crv {

template <typename underlying_t, typename tag_t>
struct is_integral<strong_type_t<underlying_t, tag_t>> : std::bool_constant<true>
{};

template <typename underlying_t, typename tag_t>
inline constexpr bool is_signed_v<strong_type_t<underlying_t, tag_t>> = is_signed_v<underlying_t>;

template <typename dst_t, typename src_underlying_t, typename src_tag_t>
constexpr auto int_cast(strong_type_t<src_underlying_t, src_tag_t> src) -> dst_t
{
    return dst_t{int_cast<typename dst_t::underlying_t>(src.underlying)};
}

template <typename lhs_underlying_t, typename lhs_tag_t, typename rhs_underlying_t, typename rhs_tag_t>
constexpr auto cmp_less(
    strong_type_t<lhs_underlying_t, lhs_tag_t> lhs, strong_type_t<rhs_underlying_t, rhs_tag_t> rhs) noexcept -> bool
{
    return cmp_less(lhs.underlying, rhs.underlying);
}

template <typename lhs_underlying_t, typename lhs_tag_t, typename rhs_underlying_t, typename rhs_tag_t>
constexpr auto cmp_greater(
    strong_type_t<lhs_underlying_t, lhs_tag_t> lhs, strong_type_t<rhs_underlying_t, rhs_tag_t> rhs) noexcept -> bool
{
    return cmp_greater(lhs.underlying, rhs.underlying);
}

namespace {

// compile-time stub+spy rounding mode
//
// All shfiters require a rounding mode, but it is only used by shr and negative shift. shl and positive shifts just
// shift left and range check.
template <typename strong_type_t> struct rounding_mode_t
{
    using underlying_t = strong_type_t::underlying_t;

    static constexpr underlying_t bias_magic = 0x11;
    static constexpr underlying_t carry_magic = 0x22;

    // mutates input predictably to prove bias was called with correct parameters
    constexpr auto bias(strong_type_t src, int_t count) const -> strong_type_t
    {
        return strong_type_t{int_cast<underlying_t>(src.underlying + count + bias_magic)};
    }

    // combines all parameters predictably to prove carry was called with correct parameters
    constexpr auto carry(strong_type_t shifted, strong_type_t unshifted, int_t count) const -> strong_type_t
    {
        return strong_type_t{int_cast<underlying_t>(shifted.underlying + unshifted.underlying + count + carry_magic)};
    }
};

template <typename underlying_t> struct death_test_t : Test
{
    using src_t = strong_type_t<underlying_t, struct src_tag_t>;
    using dst_t = strong_type_t<underlying_t, struct dst_tag_t>;
};

// ====================================================================================================================
// shr
// ====================================================================================================================

namespace shr {

template <typename underlying_t> struct test_t
{
    using src_t = strong_type_t<underlying_t, struct src_tag_t>;
    using dst_t = strong_type_t<underlying_t, struct dst_tag_t>;

    using rounding_mode_t = rounding_mode_t<src_t>;
    using sut_t = shifter_t<rounding_mode_t{}>;
    static constexpr auto sut = sut_t{};

    static constexpr auto bit_width = int_cast<int_t>(sizeof(underlying_t) * CHAR_BIT);
    static constexpr auto src = src_t{0xAA}; // arbitrary bit pattern

    static constexpr auto expected_carry(underlying_t original_src, int_t count) -> underlying_t
    {
        auto const unshifted = static_cast<underlying_t>(original_src + count + rounding_mode_t::bias_magic);
        auto const shifted = static_cast<underlying_t>(unshifted >> count);
        return static_cast<underlying_t>(shifted + unshifted + count + rounding_mode_t::carry_magic);
    }

    template <int_t count> struct dynamic_shift_test_t
    {
        // shr
        static auto const expected_shr = expected_carry(src.underlying, count);
        static_assert(expected_shr == sut.shr(src, count).underlying);
        static_assert(expected_shr == sut.template shr<dst_t>(src, count).underlying);

        // shift
        static auto const expected_shift = (count > 0) ? expected_shr : src.underlying;
        static_assert(expected_shift == sut.shift(src, -count).underlying);
        static_assert(expected_shift == sut.template shift<dst_t>(src, -count).underlying);
    };

    template <int_t count> struct static_shift_test_t
    {
        // shr
        static auto const expected_shr = expected_carry(src.underlying, count);
        static_assert(expected_shr == sut.template shr<count>(src).underlying);
        static_assert(expected_shr == sut.template shr<dst_t, count>(src).underlying);

        // shift
        static auto const expected_shift = (count > 0) ? expected_shr : src.underlying;
        static_assert(expected_shift == sut.template shift<-(count)>(src).underlying);
        static_assert(expected_shift == sut.template shift<dst_t, -(count)>(src).underlying);
    };
};

template struct test_t<int_t>;
template struct test_t<uint_t>;

template struct test_t<int_t>::dynamic_shift_test_t<0>;
template struct test_t<int_t>::dynamic_shift_test_t<test_t<int_t>::bit_width / 2>;
template struct test_t<int_t>::dynamic_shift_test_t<test_t<int_t>::bit_width - 1>;

template struct test_t<int_t>::static_shift_test_t<0>;
template struct test_t<int_t>::static_shift_test_t<test_t<int_t>::bit_width / 2>;
template struct test_t<int_t>::static_shift_test_t<test_t<int_t>::bit_width - 1>;

// --------------------------------------------------------------------------------------------------------------------
// death tests
// --------------------------------------------------------------------------------------------------------------------

#if defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG

template <typename underlying_t> struct shr_death_tests_t : Test
{
    using src_t = strong_type_t<underlying_t, struct src_tag_t>;
    using dst_t = strong_type_t<underlying_t, struct dst_tag_t>;

    using sut_t = shifter_t<rounding_mode_t<src_t>{}>;
    static constexpr auto sut = sut_t{};
    static constexpr auto src = src_t{0xAA};

    static constexpr auto src_bits = int_cast<int_t>(sizeof(src_t) * CHAR_BIT);
};

using test_types_t = Types<int_t, uint_t>;
TYPED_TEST_SUITE(shr_death_tests_t, test_types_t);

TYPED_TEST(shr_death_tests_t, shr_sym_dynamic_negative_count)
{
    EXPECT_DEBUG_DEATH(TestFixture::sut.shr(TestFixture::src, -1), "shr count must not be negative");
}

TYPED_TEST(shr_death_tests_t, shr_asym_dynamic_negative_count)
{
    using dst_t = TestFixture::dst_t;
    EXPECT_DEBUG_DEATH(TestFixture::sut.template shr<dst_t>(TestFixture::src, -1), "shr count must not be negative");
}

TYPED_TEST(shr_death_tests_t, shr_sym_dynamic_oversized_count)
{
    EXPECT_DEBUG_DEATH(
        TestFixture::sut.shr(TestFixture::src, TestFixture::src_bits), "shr count must be less than bit width");
}

TYPED_TEST(shr_death_tests_t, shr_asym_dynamic_oversized_count)
{
    using dst_t = typename TestFixture::dst_t;
    EXPECT_DEBUG_DEATH(TestFixture::sut.template shr<dst_t>(TestFixture::src, TestFixture::src_bits),
        "shr count must be less than bit width");
}

#endif

} // namespace shr

// ====================================================================================================================
// shl
// ====================================================================================================================

namespace shl {

template <typename underlying_t> struct test_t
{
    using src_t = strong_type_t<underlying_t, struct src_tag_t>;
    using dst_t = strong_type_t<underlying_t, struct fst_tag_t>;

    using sut_t = shifter_t<rounding_mode_t<src_t>{}>;
    static constexpr auto sut = sut_t{};

    static constexpr auto count = 4;

    // negative boundary
    static constexpr auto smallest_valid_underlying_for_shl = min<underlying_t>() >> count;
    static constexpr auto smallest_valid_src_for_shl = src_t{smallest_valid_underlying_for_shl};
    static_assert((smallest_valid_src_for_shl << count) == sut.shl(smallest_valid_src_for_shl, count));
    static_assert((smallest_valid_src_for_shl << count) == sut.template shl<count>(smallest_valid_src_for_shl));
    static_assert(
        dst_t{smallest_valid_src_for_shl << count} == sut.template shl<dst_t>(smallest_valid_src_for_shl, count));
    static_assert(
        dst_t{(smallest_valid_src_for_shl << count)} == sut.template shl<dst_t, count>(smallest_valid_src_for_shl));
    static_assert((smallest_valid_src_for_shl << count) == sut.shift(smallest_valid_src_for_shl, count));
    static_assert((smallest_valid_src_for_shl << count) == sut.template shift<count>(smallest_valid_src_for_shl));
    static_assert(
        dst_t{smallest_valid_src_for_shl << count} == sut.template shift<dst_t>(smallest_valid_src_for_shl, count));
    static_assert(
        dst_t{smallest_valid_src_for_shl << count} == sut.template shift<dst_t, count>(smallest_valid_src_for_shl));

    // positive boundary
    static constexpr auto largest_valid_underlying_for_shl = max<underlying_t>() >> count;
    static constexpr auto largest_valid_src_for_shl = src_t{largest_valid_underlying_for_shl};
    static_assert((largest_valid_src_for_shl << count) == sut.shl(largest_valid_src_for_shl, count));
    static_assert((largest_valid_src_for_shl << count) == sut.template shl<count>(largest_valid_src_for_shl));
    static_assert(
        dst_t{largest_valid_src_for_shl << count} == sut.template shl<dst_t>(largest_valid_src_for_shl, count));
    static_assert(
        dst_t{(largest_valid_src_for_shl << count)} == sut.template shl<dst_t, count>(largest_valid_src_for_shl));
    static_assert((largest_valid_src_for_shl << count) == sut.shift(largest_valid_src_for_shl, count));
    static_assert((largest_valid_src_for_shl << count) == sut.template shift<count>(largest_valid_src_for_shl));
    static_assert(
        dst_t{largest_valid_src_for_shl << count} == sut.template shift<dst_t>(largest_valid_src_for_shl, count));
    static_assert(
        dst_t{(largest_valid_src_for_shl << count)} == sut.template shift<dst_t, count>(largest_valid_src_for_shl));

    // zero count
    static constexpr auto zero_underlying = 37; // arbitrary
    static constexpr auto zero_src = src_t{zero_underlying};
    static constexpr auto zero_dst = dst_t{zero_underlying};
    static_assert(zero_src == sut.shl(zero_src, 0));
    static_assert(zero_src == sut.template shl<0>(zero_src));
    static_assert(zero_dst == sut.template shl<dst_t>(zero_src, 0));
    static_assert(zero_dst == sut.template shl<dst_t, 0>(zero_src));
    static_assert(zero_src == sut.shift(zero_src, 0));
    static_assert(zero_src == sut.template shift<0>(zero_src));
    static_assert(zero_dst == sut.template shift<dst_t>(zero_src, 0));
    static_assert(zero_dst == sut.template shift<dst_t, 0>(zero_src));
};

template struct test_t<int_t>;
template struct test_t<uint_t>;

// --------------------------------------------------------------------------------------------------------------------
// death tests
// --------------------------------------------------------------------------------------------------------------------

#if defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG

template <typename t_underlying_t> struct shl_death_tests_t : Test
{
    using underlying_t = t_underlying_t;
    using src_t = strong_type_t<underlying_t, struct src_tag_t>;
    using dst_t = strong_type_t<underlying_t, struct fst_tag_t>;

    using sut_t = shifter_t<rounding_mode_t<src_t>{}>;
    static constexpr auto sut = sut_t{};

    static constexpr auto count = 4;

    static constexpr auto largest_invalid_underlying_for_shl = (min<underlying_t>() >> count) - 1;
    static constexpr auto smallest_invalid_underlying_for_shl = (max<underlying_t>() >> count) + 1;

    static constexpr auto largest_invalid_src_for_shl = src_t{largest_invalid_underlying_for_shl};
    static constexpr auto smallest_invalid_src_for_shl = src_t{smallest_invalid_underlying_for_shl};

    static constexpr auto dst_bits = int_cast<int_t>(sizeof(src_t) * CHAR_BIT);
};

using test_types_t = Types<int_t, uint_t>;
TYPED_TEST_SUITE(shl_death_tests_t, test_types_t);

TYPED_TEST(shl_death_tests_t, shl_sym_dynamic_negative_count)
{
    using src_t = typename TestFixture::src_t;

    EXPECT_DEBUG_DEATH(TestFixture::sut.shl(src_t{100}, -1), "shl count must not be negative");
}

TYPED_TEST(shl_death_tests_t, shl_asym_dynamic_negative_count)
{
    using src_t = TestFixture::src_t;
    using dst_t = TestFixture::dst_t;

    EXPECT_DEBUG_DEATH(TestFixture::sut.template shl<dst_t>(src_t{100}, -1), "shl count must not be negative");
}

TYPED_TEST(shl_death_tests_t, shl_sym_dynamic_negative_overflow)
{
    if constexpr (!is_signed_v<typename TestFixture::underlying_t>)
    {
        GTEST_SUCCEED();
        return;
    }

    EXPECT_DEBUG_DEATH(
        TestFixture::sut.shl(TestFixture::largest_invalid_src_for_shl, TestFixture::count), "negative overflow");
}

TYPED_TEST(shl_death_tests_t, shl_sym_static_negative_overflow)
{
    if constexpr (!is_signed_v<typename TestFixture::underlying_t>)
    {
        GTEST_SUCCEED();
        return;
    }

    EXPECT_DEBUG_DEATH(TestFixture::sut.template shl<TestFixture::count>(TestFixture::largest_invalid_src_for_shl),
        "negative overflow");
}

TYPED_TEST(shl_death_tests_t, shl_asym_dynamic_negative_overflow)
{
    if constexpr (!is_signed_v<typename TestFixture::underlying_t>)
    {
        GTEST_SUCCEED();
        return;
    }

    using dst_t = typename TestFixture::dst_t;

    EXPECT_DEBUG_DEATH(
        TestFixture::sut.template shl<dst_t>(TestFixture::largest_invalid_src_for_shl, TestFixture::count),
        "negative overflow");
}

TYPED_TEST(shl_death_tests_t, shl_asym_static_negative_overflow)
{
    if constexpr (!is_signed_v<typename TestFixture::underlying_t>)
    {
        GTEST_SUCCEED();
        return;
    }

    using dst_t = TestFixture::dst_t;

    EXPECT_DEBUG_DEATH(
        (TestFixture::sut.template shl<dst_t, TestFixture::count>(TestFixture::largest_invalid_src_for_shl)),
        "negative overflow");
}

TYPED_TEST(shl_death_tests_t, shift_sym_dynamic_negative_overflow)
{
    if constexpr (!is_signed_v<typename TestFixture::underlying_t>)
    {
        GTEST_SUCCEED();
        return;
    }

    EXPECT_DEBUG_DEATH(
        TestFixture::sut.shift(TestFixture::largest_invalid_src_for_shl, TestFixture::count), "negative overflow");
}

TYPED_TEST(shl_death_tests_t, shift_sym_static_negative_overflow)
{
    if constexpr (!is_signed_v<typename TestFixture::underlying_t>)
    {
        GTEST_SUCCEED();
        return;
    }

    EXPECT_DEBUG_DEATH(TestFixture::sut.template shift<TestFixture::count>(TestFixture::largest_invalid_src_for_shl),
        "negative overflow");
}

TYPED_TEST(shl_death_tests_t, shift_asym_dynamic_negative_overflow)
{
    if constexpr (!is_signed_v<typename TestFixture::underlying_t>)
    {
        GTEST_SUCCEED();
        return;
    }

    using dst_t = typename TestFixture::dst_t;

    EXPECT_DEBUG_DEATH(
        TestFixture::sut.template shift<dst_t>(TestFixture::largest_invalid_src_for_shl, TestFixture::count),
        "negative overflow");
}

TYPED_TEST(shl_death_tests_t, shift_asym_static_negative_overflow)
{
    if constexpr (!is_signed_v<typename TestFixture::underlying_t>)
    {
        GTEST_SUCCEED();
        return;
    }

    using dst_t = typename TestFixture::dst_t;

    EXPECT_DEBUG_DEATH(
        (TestFixture::sut.template shift<dst_t, TestFixture::count>(TestFixture::largest_invalid_src_for_shl)),
        "negative overflow");
}

TYPED_TEST(shl_death_tests_t, shl_sym_dynamic_positive_overflow)
{
    EXPECT_DEBUG_DEATH(
        TestFixture::sut.shl(TestFixture::smallest_invalid_src_for_shl, TestFixture::count), "positive overflow");
}

TYPED_TEST(shl_death_tests_t, shl_sym_static_positive_overflow)
{
    EXPECT_DEBUG_DEATH(TestFixture::sut.template shl<TestFixture::count>(TestFixture::smallest_invalid_src_for_shl),
        "positive overflow");
}

TYPED_TEST(shl_death_tests_t, shl_asym_dynamic_positive_overflow)
{
    using dst_t = typename TestFixture::dst_t;

    EXPECT_DEBUG_DEATH(
        TestFixture::sut.template shl<dst_t>(TestFixture::smallest_invalid_src_for_shl, TestFixture::count),
        "positive overflow");
}

TYPED_TEST(shl_death_tests_t, shl_asym_static_positive_overflow)
{
    using dst_t = typename TestFixture::dst_t;

    EXPECT_DEBUG_DEATH(
        (TestFixture::sut.template shl<dst_t, TestFixture::count>(TestFixture::smallest_invalid_src_for_shl)),
        "positive overflow");
}

TYPED_TEST(shl_death_tests_t, shift_sym_dynamic_positive_overflow)
{
    EXPECT_DEBUG_DEATH(
        TestFixture::sut.shift(TestFixture::smallest_invalid_src_for_shl, TestFixture::count), "positive overflow");
}

TYPED_TEST(shl_death_tests_t, shift_sym_static_positive_overflow)
{
    EXPECT_DEBUG_DEATH(TestFixture::sut.template shift<TestFixture::count>(TestFixture::smallest_invalid_src_for_shl),
        "positive overflow");
}

TYPED_TEST(shl_death_tests_t, shift_asym_dynamic_positive_overflow)
{
    using dst_t = typename TestFixture::dst_t;

    EXPECT_DEBUG_DEATH(
        TestFixture::sut.template shift<dst_t>(TestFixture::smallest_invalid_src_for_shl, TestFixture::count),
        "positive overflow");
}

TYPED_TEST(shl_death_tests_t, shift_asym_static_positive_overflow)
{
    using dst_t = typename TestFixture::dst_t;

    EXPECT_DEBUG_DEATH(
        (TestFixture::sut.template shift<dst_t, TestFixture::count>(TestFixture::smallest_invalid_src_for_shl)),
        "positive overflow");
}

TYPED_TEST(shl_death_tests_t, shl_sym_dynamic_oversized_count)
{
    using src_t = typename TestFixture::src_t;

    EXPECT_DEBUG_DEATH(
        TestFixture::sut.shl(src_t{100}, TestFixture::dst_bits), "shl count larger than target bit width");
}

TYPED_TEST(shl_death_tests_t, shl_asym_dynamic_oversized_count)
{
    using src_t = typename TestFixture::src_t;
    using dst_t = typename TestFixture::dst_t;

    EXPECT_DEBUG_DEATH(TestFixture::sut.template shl<dst_t>(src_t{100}, TestFixture::dst_bits),
        "shl count larger than target bit width");
}

#endif

} // namespace shl

} // namespace
} // namespace crv
