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
    constexpr auto operator==(strong_type_t const& other) const noexcept -> bool  = default;
};

using underlying_t = int_t;
using src_t        = strong_type_t<underlying_t, struct src_tag_t>;
using dst_t        = strong_type_t<underlying_t, struct dst_tag_t>;

} // namespace crv

namespace std {

template <typename underlying_t, typename tag_t>
struct numeric_limits<crv::strong_type_t<underlying_t, tag_t>> : numeric_limits<underlying_t>
{
    static constexpr bool is_specialized = true;

    using src_t = numeric_limits<underlying_t>;
    using dst_t = crv::strong_type_t<underlying_t, tag_t>;

    static constexpr auto min() noexcept -> dst_t { return dst_t{src_t::min()}; }
    static constexpr auto max() noexcept -> dst_t { return dst_t{src_t::max()}; }
    static constexpr auto lowest() noexcept -> dst_t { return dst_t{src_t::lowest()}; }
    static constexpr auto epsilon() noexcept -> dst_t { return dst_t{src_t::epsilon()}; }
    static constexpr auto round_error() noexcept -> dst_t { return dst_t{src_t::round_error()}; }
    static constexpr auto infinity() noexcept -> dst_t { return dst_t{src_t::infinity()}; }
    static constexpr auto quiet_NaN() noexcept -> dst_t { return dst_t{src_t::quiet_NaN()}; }
    static constexpr auto signaling_NaN() noexcept -> dst_t { return dst_t{src_t::signaling_NaN()}; }
    static constexpr auto denorm_min() noexcept -> dst_t { return dst_t{src_t::denorm_min()}; }
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
constexpr auto cmp_less(strong_type_t<lhs_underlying_t, lhs_tag_t> lhs,
                        strong_type_t<rhs_underlying_t, rhs_tag_t> rhs) noexcept -> bool
{
    return cmp_less(lhs.underlying, rhs.underlying);
}

template <typename lhs_underlying_t, typename lhs_tag_t, typename rhs_underlying_t, typename rhs_tag_t>
constexpr auto cmp_greater(strong_type_t<lhs_underlying_t, lhs_tag_t> lhs,
                           strong_type_t<rhs_underlying_t, rhs_tag_t> rhs) noexcept -> bool
{
    return cmp_greater(lhs.underlying, rhs.underlying);
}

struct rounding_mode_t
{
    int_t expected_src{};
    int_t expected_count{};
    int_t bias_result{};

    constexpr auto bias(auto src, auto count) const -> auto
    {
        if (src.underlying != expected_src) throw "bias() received incorrect src";
        if (count != expected_count) throw "bias() received incorrect count";
        return decltype(src){bias_result};
    }

    int_t expected_shifted{};
    int_t expected_unshifted{};
    int_t carry_result{};

    constexpr auto carry(auto shifted, auto unshifted, auto count) const -> auto
    {
        if (shifted.underlying != expected_shifted) throw "carry() received incorrect shifted";
        if (unshifted.underlying != expected_unshifted) throw "carry() received incorrect unshifted";
        if (count != expected_count) throw "carry() received incorrect count";
        return decltype(shifted){carry_result};
    }
};

namespace {

// ====================================================================================================================
// shr
// ====================================================================================================================

namespace shr {

struct shr_test_t
{
    static constexpr rounding_mode_t rounding_mode{
        .expected_src       = 100,
        .expected_count     = 2,
        .bias_result        = 104,
        .expected_shifted   = 104 >> 2,
        .expected_unshifted = 104,
        .carry_result       = 27,
    };

    constexpr auto test_shr_sym_dynamic() const
    {
        constexpr auto sut    = shifter_t{rounding_mode};
        constexpr auto result = sut.shr(src_t{100}, 2);
        if (result.underlying != (27)) throw "test_shr_sym_dynamic";
        return true;
    }

    constexpr auto test_shr_sym_static() const
    {
        constexpr auto sut    = shifter_t{rounding_mode};
        constexpr auto result = sut.shr<2>(src_t{100});
        if (result.underlying != (27)) throw "test_shr_sym_static";
        return true;
    }

    constexpr auto test_shr_asym_dynamic() const
    {
        constexpr auto sut    = shifter_t{rounding_mode};
        constexpr auto result = sut.shr<dst_t>(src_t{100}, 2);
        if (result.underlying != (27)) throw "test_shr_asym_dynamic";
        return true;
    }

    constexpr auto test_shr_asym_static() const
    {
        constexpr auto sut    = shifter_t{rounding_mode};
        constexpr auto result = sut.shr<dst_t, 2>(src_t{100});
        if (result.underlying != (27)) throw "test_shr_asym_static";
        return true;
    }

    constexpr auto test_shift_sym_dynamic_positive_count() const
    {
        constexpr auto sut    = shifter_t{rounding_mode};
        constexpr auto result = sut.shift(src_t{100}, 2);
        if (result.underlying != (27)) throw "test_shift_sym_dynamic_positive_count";
        return true;
    }

    constexpr auto test_shift_sym_static_positive_count() const
    {
        constexpr auto sut    = shifter_t{rounding_mode};
        constexpr auto result = sut.shift<2>(src_t{100});
        if (result.underlying != (27)) throw "test_shift_sym_static_positive_count";
        return true;
    }

    constexpr auto test_shift_asym_dynamic_positive_count() const
    {
        constexpr auto sut    = shifter_t{rounding_mode};
        constexpr auto result = sut.shift<dst_t>(src_t{100}, 2);
        if (result.underlying != (27)) throw "test_shift_asym_dynamic_positive_count";
        return true;
    }

    constexpr auto test_shift_asym_static_positive_count() const
    {
        constexpr auto sut    = shifter_t{rounding_mode};
        constexpr auto result = sut.shift<dst_t, 2>(src_t{100});
        if (result.underlying != (27)) throw "test_shift_asym_static_positive_count";
        return true;
    }
};

constexpr auto shr_test = shr_test_t{};

static_assert(shr_test.test_shr_sym_dynamic(), "shifter_t: test_shr_sym_dynamic failed");
static_assert(shr_test.test_shr_sym_static(), "shifter_t: test_shr_sym_static failed");
static_assert(shr_test.test_shr_asym_dynamic(), "shifter_t: test_shr_asym_dynamic failed");
static_assert(shr_test.test_shr_asym_static(), "shifter_t: test_shr_asym_static failed");

static_assert(shr_test.test_shift_sym_dynamic_positive_count(), "shifter_t: test_shift_sym_dynamic_positive_count failed");
static_assert(shr_test.test_shift_sym_static_positive_count(), "shifter_t: test_shift_sym_static_positive_count failed");
static_assert(shr_test.test_shift_asym_dynamic_positive_count(), "shifter_t: test_shift_asym_dynamic_positive_count failed");
static_assert(shr_test.test_shift_asym_static_positive_count(), "shifter_t: test_shift_asym_static_positive_count failed");

TEST(shifter_death_test, shr_sym_dynamic_zero_count)
{
    constexpr auto sut = shifter_t<rounding_mode_t>{};

    EXPECT_DEATH(sut.shr(src_t{100}, 0), "shr count must be positive");
}

TEST(shifter_death_test, shr_asym_dynamic_zero_count)
{
    constexpr auto sut = shifter_t<rounding_mode_t>{};

    EXPECT_DEATH(sut.shr<dst_t>(src_t{100}, 0), "shr count must be positive");
}

TEST(shifter_death_test, shr_sym_dynamic_oversized_count)
{
    constexpr auto sut      = shifter_t<crv::rounding_mode_t>{};
    constexpr auto src_bits = int_cast<int_t>(sizeof(src_t) * CHAR_BIT);

    EXPECT_DEATH(sut.shr(src_t{100}, src_bits), "shr count must be less than bit width");
}

TEST(shifter_death_test, shr_asym_dynamic_oversized_count)
{
    constexpr auto sut      = shifter_t<crv::rounding_mode_t>{};
    constexpr auto src_bits = int_cast<int_t>(sizeof(src_t) * CHAR_BIT);

    EXPECT_DEATH(sut.shr<dst_t>(src_t{100}, src_bits), "shr count must be less than bit width");
}

} // namespace shr

// ====================================================================================================================
// shl
// ====================================================================================================================

namespace shl {

constexpr auto const largest_invalid_underlying_for_shl_4  = (min<underlying_t>() >> 4) - 1;
constexpr auto const smallest_valid_underlying_for_shl_4   = min<underlying_t>() >> 4;
constexpr auto const largest_valid_underlying_for_shl_4    = max<underlying_t>() >> 4;
constexpr auto const smallest_invalid_underlying_for_shl_4 = (max<underlying_t>() >> 4) + 1;

constexpr auto largest_invalid_src_for_shl_4  = src_t{largest_invalid_underlying_for_shl_4};
constexpr auto smallest_valid_src_for_shl_4   = src_t{smallest_valid_underlying_for_shl_4};
constexpr auto largest_valid_src_for_shl_4    = src_t{largest_valid_underlying_for_shl_4};
constexpr auto smallest_invalid_src_for_shl_4 = src_t{smallest_invalid_underlying_for_shl_4};

// --------------------------------------------------------------------------------------------------------------------
// negative boundary
// --------------------------------------------------------------------------------------------------------------------

constexpr auto test_shl_sym_dynamic_negative_boundary() noexcept -> bool
{
    constexpr auto sut = shifter_t<rounding_mode_t>{};

    static_assert((smallest_valid_src_for_shl_4 << 4) == sut.shl(smallest_valid_src_for_shl_4, 4));

    return true;
}
static_assert(test_shl_sym_dynamic_negative_boundary(), "shifter_t: test_shl_sym_dynamic_negative_boundary failed");

constexpr auto test_shl_sym_static_negative_boundary() noexcept -> bool
{
    constexpr auto sut = shifter_t<rounding_mode_t>{};

    static_assert((smallest_valid_src_for_shl_4 << 4) == sut.shl<4>(smallest_valid_src_for_shl_4));

    return true;
}
static_assert(test_shl_sym_static_negative_boundary(), "shifter_t: test_shl_sym_static_negative_boundary failed");

constexpr auto test_shl_asym_dynamic_negative_boundary() noexcept -> bool
{
    constexpr auto sut = shifter_t<rounding_mode_t>{};

    static_assert(dst_t{smallest_valid_src_for_shl_4 << 4} == sut.shl<dst_t>(smallest_valid_src_for_shl_4, 4));

    return true;
}
static_assert(test_shl_asym_dynamic_negative_boundary(), "shifter_t: test_shl_asym_dynamic_negative_boundary failed");

constexpr auto test_shl_asym_static_negative_boundary() noexcept -> bool
{
    constexpr auto sut = shifter_t<rounding_mode_t>{};

    static_assert(dst_t{(smallest_valid_src_for_shl_4 << 4)} == sut.shl<dst_t, 4>(smallest_valid_src_for_shl_4));

    return true;
}
static_assert(test_shl_asym_static_negative_boundary(), "shifter_t: test_shl_asym_static_negative_boundary failed");

// --------------------------------------------------------------------------------------------------------------------
// positive boundary
// --------------------------------------------------------------------------------------------------------------------

constexpr auto test_shl_sym_dynamic_positive_boundary() noexcept -> bool
{
    constexpr auto sut = shifter_t<rounding_mode_t>{};

    static_assert((largest_valid_src_for_shl_4 << 4) == sut.shl(largest_valid_src_for_shl_4, 4));

    return true;
}
static_assert(test_shl_sym_dynamic_positive_boundary(), "shifter_t: test_shl_sym_dynamic_positive_boundary failed");

constexpr auto test_shl_sym_static_positive_boundary() noexcept -> bool
{
    constexpr auto sut = shifter_t<rounding_mode_t>{};

    static_assert((largest_valid_src_for_shl_4 << 4) == sut.shl<4>(largest_valid_src_for_shl_4));

    return true;
}
static_assert(test_shl_sym_static_positive_boundary(), "shifter_t: test_shl_sym_static_positive_boundary failed");

constexpr auto test_shl_asym_dynamic_positive_boundary() noexcept -> bool
{
    constexpr auto sut = shifter_t<rounding_mode_t>{};

    static_assert(dst_t{largest_valid_src_for_shl_4 << 4} == sut.shl<dst_t>(largest_valid_src_for_shl_4, 4));

    return true;
}
static_assert(test_shl_asym_dynamic_positive_boundary(), "shifter_t: test_shl_asym_dynamic_positive_boundary failed");

constexpr auto test_shl_asym_static_positive_boundary() noexcept -> bool
{
    constexpr auto sut = shifter_t<rounding_mode_t>{};

    static_assert(dst_t{(largest_valid_src_for_shl_4 << 4)} == sut.shl<dst_t, 4>(largest_valid_src_for_shl_4));

    return true;
}
static_assert(test_shl_asym_static_positive_boundary(), "shifter_t: test_shl_asym_static_positive_boundary failed");

// --------------------------------------------------------------------------------------------------------------------
// negative count
// --------------------------------------------------------------------------------------------------------------------

constexpr auto test_shift_sym_dynamic_negative_count() noexcept -> bool
{
    constexpr auto sut = shifter_t<rounding_mode_t>{};

    static_assert((smallest_valid_src_for_shl_4 << 4) == sut.shift(smallest_valid_src_for_shl_4, -4));

    return true;
}
static_assert(test_shift_sym_dynamic_negative_count(), "shifter_t: test_shift_sym_dynamic_negative_count failed");

constexpr auto test_shift_sym_static_negative_count() noexcept -> bool
{
    constexpr auto sut = shifter_t<rounding_mode_t>{};

    static_assert((smallest_valid_src_for_shl_4 << 4) == sut.shift<-4>(smallest_valid_src_for_shl_4));

    return true;
}
static_assert(test_shift_sym_static_negative_count(), "shifter_t: test_shift_sym_static_negative_count failed");

constexpr auto test_shift_asym_dynamic_negative_count() noexcept -> bool
{
    constexpr auto sut = shifter_t<rounding_mode_t>{};

    static_assert(dst_t{smallest_valid_src_for_shl_4 << 4} == sut.shift<dst_t>(smallest_valid_src_for_shl_4, -4));

    return true;
}
static_assert(test_shift_asym_dynamic_negative_count(), "shifter_t: test_shift_asym_dynamic_negative_count failed");

constexpr auto test_shift_asym_static_negative_count() noexcept -> bool
{
    constexpr auto sut = shifter_t<rounding_mode_t>{};

    static_assert(dst_t{smallest_valid_src_for_shl_4 << 4} == sut.shift<dst_t, -4>(smallest_valid_src_for_shl_4));

    return true;
}
static_assert(test_shift_asym_static_negative_count(), "shifter_t: test_shift_asym_static_negative_count failed");

// --------------------------------------------------------------------------------------------------------------------
// zero count
// --------------------------------------------------------------------------------------------------------------------

constexpr auto test_shl_sym_dynamic_zero_count() noexcept -> bool
{
    constexpr auto sut = shifter_t<rounding_mode_t>{};

    static_assert(src_t{42} == sut.shl(src_t{42}, 0));

    return true;
}
static_assert(test_shl_sym_dynamic_zero_count(), "shifter_t: test_shl_sym_dynamic_zero_count failed");

constexpr auto test_shl_sym_static_zero_count() noexcept -> bool
{
    constexpr auto sut = shifter_t<rounding_mode_t>{};

    static_assert(src_t{42} == sut.shl<0>(src_t{42}));

    return true;
}
static_assert(test_shl_sym_static_zero_count(), "shifter_t: test_shl_sym_static_zero_count failed");

constexpr auto test_shl_asym_dynamic_zero_count() noexcept -> bool
{
    constexpr auto sut = shifter_t<rounding_mode_t>{};

    static_assert(dst_t{42} == sut.shl<dst_t>(src_t{42}, 0));

    return true;
}
static_assert(test_shl_asym_dynamic_zero_count(), "shifter_t: test_shl_asym_dynamic_zero_count failed");

constexpr auto test_shl_asym_static_zero_count() noexcept -> bool
{
    constexpr auto sut = shifter_t<rounding_mode_t>{};

    static_assert(dst_t{42} == sut.shl<dst_t, 0>(src_t{42}));

    return true;
}
static_assert(test_shl_asym_static_zero_count(), "shifter_t: test_shl_asym_static_zero_count failed");

constexpr auto test_shift_sym_dynamic_zero_count() noexcept -> bool
{
    constexpr auto sut = shifter_t<rounding_mode_t>{};

    static_assert(src_t{42} == sut.shift(src_t{42}, 0));

    return true;
}
static_assert(test_shift_sym_dynamic_zero_count(), "shifter_t: test_shift_sym_dynamic_zero_count failed");

constexpr auto test_shift_sym_static_zero_count() noexcept -> bool
{
    constexpr auto sut = shifter_t<rounding_mode_t>{};

    static_assert(src_t{42} == sut.shift<0>(src_t{42}));

    return true;
}
static_assert(test_shift_sym_static_zero_count(), "shifter_t: test_shift_sym_static_zero_count failed");

constexpr auto test_shift_asym_dynamic_zero_count() noexcept -> bool
{
    constexpr auto sut = shifter_t<rounding_mode_t>{};

    static_assert(dst_t{42} == sut.shift<dst_t>(src_t{42}, 0));

    return true;
}
static_assert(test_shift_asym_dynamic_zero_count(), "shifter_t: test_shift_asym_dynamic_zero_count failed");

constexpr auto test_shift_asym_static_zero_count() noexcept -> bool
{
    constexpr auto sut = shifter_t<rounding_mode_t>{};

    static_assert(dst_t{42} == sut.shift<dst_t, 0>(src_t{42}));

    return true;
}
static_assert(test_shift_asym_static_zero_count(), "shifter_t: test_shift_asym_static_zero_count failed");

TEST(shifter_death_test, shl_sym_dynamic_negative_count)
{
    constexpr auto sut = shifter_t<crv::rounding_mode_t>{};

    EXPECT_DEATH(sut.shl(src_t{100}, -1), "shl count must not be negative");
}

TEST(shifter_death_test, shl_asym_dynamic_negative_count)
{
    constexpr auto sut = shifter_t<crv::rounding_mode_t>{};

    EXPECT_DEATH(sut.shl<dst_t>(src_t{100}, -1), "shl count must not be negative");
}

// --------------------------------------------------------------------------------------------------------------------
// negative overflow
// --------------------------------------------------------------------------------------------------------------------

TEST(shifter_death_test, shl_sym_dynamic_negative_overflow)
{
    constexpr auto sut = shifter_t<crv::rounding_mode_t>{};

    EXPECT_DEATH(sut.shl(largest_invalid_src_for_shl_4, 4), "negative overflow");
}

TEST(shifter_death_test, shl_sym_static_negative_overflow)
{
    constexpr auto sut = shifter_t<crv::rounding_mode_t>{};

    EXPECT_DEATH(sut.shl<4>(largest_invalid_src_for_shl_4), "negative overflow");
}

TEST(shifter_death_test, shl_asym_dynamic_negative_overflow)
{
    constexpr auto sut = shifter_t<crv::rounding_mode_t>{};

    EXPECT_DEATH(sut.shl<dst_t>(largest_invalid_src_for_shl_4, 4), "negative overflow");
}

TEST(shifter_death_test, shl_asym_static_negative_overflow)
{
    constexpr auto sut = shifter_t<crv::rounding_mode_t>{};

    EXPECT_DEATH((sut.shl<dst_t, 4>(largest_invalid_src_for_shl_4)), "negative overflow");
}

TEST(shifter_death_test, shift_sym_dynamic_negative_overflow)
{
    constexpr auto sut = shifter_t<crv::rounding_mode_t>{};

    EXPECT_DEATH(sut.shift(largest_invalid_src_for_shl_4, -4), "negative overflow");
}

TEST(shifter_death_test, shift_sym_static_negative_overflow)
{
    constexpr auto sut = shifter_t<crv::rounding_mode_t>{};

    EXPECT_DEATH(sut.shift<-4>(largest_invalid_src_for_shl_4), "negative overflow");
}

TEST(shifter_death_test, shift_asym_dynamic_negative_overflow)
{
    constexpr auto sut = shifter_t<crv::rounding_mode_t>{};

    EXPECT_DEATH(sut.shift<dst_t>(largest_invalid_src_for_shl_4, -4), "negative overflow");
}

TEST(shifter_death_test, shift_asym_static_negative_overflow)
{
    constexpr auto sut = shifter_t<crv::rounding_mode_t>{};

    EXPECT_DEATH((sut.shift<dst_t, -4>(largest_invalid_src_for_shl_4)), "negative overflow");
}

// --------------------------------------------------------------------------------------------------------------------
// positive overflow
// --------------------------------------------------------------------------------------------------------------------

TEST(shifter_death_test, shl_sym_dynamic_positive_overflow)
{
    constexpr auto sut = shifter_t<crv::rounding_mode_t>{};

    EXPECT_DEATH(sut.shl(smallest_invalid_src_for_shl_4, 4), "positive overflow");
}

TEST(shifter_death_test, shl_sym_static_positive_overflow)
{
    constexpr auto sut = shifter_t<crv::rounding_mode_t>{};

    EXPECT_DEATH(sut.shl<4>(smallest_invalid_src_for_shl_4), "positive overflow");
}

TEST(shifter_death_test, shl_asym_dynamic_positive_overflow)
{
    constexpr auto sut = shifter_t<crv::rounding_mode_t>{};

    EXPECT_DEATH(sut.shl<dst_t>(smallest_invalid_src_for_shl_4, 4), "positive overflow");
}

TEST(shifter_death_test, shl_asym_static_positive_overflow)
{
    constexpr auto sut = shifter_t<crv::rounding_mode_t>{};

    EXPECT_DEATH((sut.shl<dst_t, 4>(smallest_invalid_src_for_shl_4)), "positive overflow");
}

TEST(shifter_death_test, shift_sym_dynamic_positive_overflow)
{
    constexpr auto sut = shifter_t<crv::rounding_mode_t>{};

    EXPECT_DEATH(sut.shift(smallest_invalid_src_for_shl_4, -4), "positive overflow");
}

TEST(shifter_death_test, shift_sym_static_positive_overflow)
{
    constexpr auto sut = shifter_t<crv::rounding_mode_t>{};

    EXPECT_DEATH(sut.shift<-4>(smallest_invalid_src_for_shl_4), "positive overflow");
}

TEST(shifter_death_test, shift_asym_dynamic_positive_overflow)
{
    constexpr auto sut = shifter_t<crv::rounding_mode_t>{};

    EXPECT_DEATH(sut.shift<dst_t>(smallest_invalid_src_for_shl_4, -4), "positive overflow");
}

TEST(shifter_death_test, shift_asym_static_positive_overflow)
{
    constexpr auto sut = shifter_t<crv::rounding_mode_t>{};

    EXPECT_DEATH((sut.shift<dst_t, -4>(smallest_invalid_src_for_shl_4)), "positive overflow");
}

// --------------------------------------------------------------------------------------------------------------------
// oversized count
// --------------------------------------------------------------------------------------------------------------------

TEST(shifter_death_test, shl_sym_dynamic_oversized_count)
{
    constexpr auto sut      = shifter_t<crv::rounding_mode_t>{};
    constexpr auto dst_bits = int_cast<int_t>(sizeof(src_t) * CHAR_BIT);

    EXPECT_DEATH(sut.shl(src_t{100}, dst_bits), "shl count larger than target bit width");
}

TEST(shifter_death_test, shl_asym_dynamic_oversized_count)
{
    constexpr auto sut      = shifter_t<crv::rounding_mode_t>{};
    constexpr auto dst_bits = int_cast<int_t>(sizeof(dst_t) * CHAR_BIT);

    EXPECT_DEATH(sut.shl<dst_t>(src_t{100}, dst_bits), "shl count larger than target bit width");
}

} // namespace shl

} // namespace
} // namespace crv
