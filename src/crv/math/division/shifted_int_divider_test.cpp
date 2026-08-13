// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "shifted_int_divider.hpp"
#include <crv/math/division/qr_pair.hpp>
#include <crv/test/test.hpp>

namespace crv::division {
namespace {

// ====================================================================================================================
// Test Doubles
// ====================================================================================================================

constexpr auto default_shift = 3;
constexpr auto expected_tracking_rounding_mode_id = 8675309;
constexpr auto poisoned_rounded_sentinel = 0xBAD;

// dummy rounding mode that carries a token to prove the adapter forwards it properly
template <unsigned_integral narrow_t> struct tracking_rounding_mode_t
{
    using wide_t = widened_t<narrow_t>;

    int_t id = 0;

    constexpr auto bias(wide_t dividend, narrow_t) const noexcept -> wide_t;
    constexpr auto carry(wide_t quotient, narrow_t, narrow_t) const noexcept -> wide_t;
};

// fake divider that tracks the rounding mode id
template <unsigned_integral t_narrow_t> struct tracking_wide_divider_t
{
    using narrow_t = t_narrow_t;
    using wide_t = widened_t<narrow_t>;

    constexpr auto operator()(wide_t dividend, narrow_t divisor, tracking_rounding_mode_t<narrow_t> rm) const noexcept
        -> wide_t
    {
        if (rm.id != expected_tracking_rounding_mode_id) return static_cast<wide_t>(poisoned_rounded_sentinel);

        auto const actual = dividend / divisor;
        assert(actual != poisoned_rounded_sentinel);
        return int_cast<wide_t>(actual);
    }
};

// ====================================================================================================================
// Tests
// ====================================================================================================================

// test that empty base optimization is enabled
using ebo_sut_t
    = shifted_int_divider_t<tracking_wide_divider_t<uint32_t>, default_shift, int32_t, int32_t, int32_t, true>;
static_assert(sizeof(ebo_sut_t) == 1, "shifted_int_divider_t should not add overhead for empty dividers");

// --------------------------------------------------------------------------------------------------------------------
// Heterogeneous
// --------------------------------------------------------------------------------------------------------------------

struct heterogeneous_test_t
{
    using narrow_t = uint32_t;
    using rounding_mode_t = tracking_rounding_mode_t<narrow_t>;
    using wide_divider_t = tracking_wide_divider_t<narrow_t>;

    static constexpr auto rounding_mode = rounding_mode_t{.id = expected_tracking_rounding_mode_id};

    using sut_t = shifted_int_divider_t<wide_divider_t, default_shift, int16_t, uint32_t, int8_t, true>;
    static constexpr auto sut = sut_t{};

    // fits: (100 << 3) / -2 = 800 / -2 = -400
    static_assert(sut(uint32_t{100}, int8_t{-2}, rounding_mode) == int16_t{-400});

    // bounds test: clamps to min<int16_t>
    static_assert(sut(uint32_t{10000}, int8_t{-1}, rounding_mode) == min<int16_t>());

    // bounds test: clamps to max<int16_t>
    static_assert(sut(uint32_t{10000}, int8_t{1}, rounding_mode) == max<int16_t>());

    // unsigned output boundary test
    using unsigned_out_sut_t = shifted_int_divider_t<wide_divider_t, default_shift, uint16_t, int32_t, int32_t, true>;
    static constexpr auto u_sut = unsigned_out_sut_t{};

    // negative mathematical result correctly clamped to 0 for unsigned output type
    static_assert(u_sut(int32_t{100}, int32_t{-2}, rounding_mode) == uint16_t{0});
};

// --------------------------------------------------------------------------------------------------------------------
// Wide Dividend
// --------------------------------------------------------------------------------------------------------------------

struct wide_dividend_test_t
{
    using narrow_t = uint64_t;
    using wide_t = uint128_t;
    using signed_wide_t = int128_t;

    using rounding_mode_t = tracking_rounding_mode_t<narrow_t>;
    using wide_divider_t = tracking_wide_divider_t<narrow_t>;

    static constexpr auto rounding_mode = rounding_mode_t{.id = expected_tracking_rounding_mode_id};
    static constexpr auto shift = 0;

    using signed_signed_sut_t = shifted_int_divider_t<wide_divider_t, shift, int64_t, signed_wide_t, int64_t, true>;

    using signed_unsigned_sut_t = shifted_int_divider_t<wide_divider_t, shift, int64_t, signed_wide_t, uint64_t, true>;

    using unsigned_signed_sut_t = shifted_int_divider_t<wide_divider_t, shift, int64_t, wide_t, int64_t, true>;

    static constexpr auto signed_signed_sut = signed_signed_sut_t{};
    static constexpr auto signed_unsigned_sut = signed_unsigned_sut_t{};
    static constexpr auto unsigned_signed_sut = unsigned_signed_sut_t{};

    // s128 / s64, all sign quadrants
    static_assert(signed_signed_sut(signed_wide_t{100}, int64_t{2}, rounding_mode) == int64_t{50});
    static_assert(signed_signed_sut(signed_wide_t{-100}, int64_t{2}, rounding_mode) == int64_t{-50});
    static_assert(signed_signed_sut(signed_wide_t{100}, int64_t{-2}, rounding_mode) == int64_t{-50});
    static_assert(signed_signed_sut(signed_wide_t{-100}, int64_t{-2}, rounding_mode) == int64_t{50});

    // s128 / u64
    static_assert(signed_unsigned_sut(signed_wide_t{100}, uint64_t{2}, rounding_mode) == int64_t{50});
    static_assert(signed_unsigned_sut(signed_wide_t{-100}, uint64_t{2}, rounding_mode) == int64_t{-50});

    // u128 / s64
    static_assert(unsigned_signed_sut(wide_t{100}, int64_t{2}, rounding_mode) == int64_t{50});
    static_assert(unsigned_signed_sut(wide_t{100}, int64_t{-2}, rounding_mode) == int64_t{-50});

    // exact signed-output minimum; must not attempt to materialize +2^63 as int64_t
    static_assert(signed_unsigned_sut(signed_wide_t{min<int64_t>()}, uint64_t{1}, rounding_mode) == min<int64_t>());

    // divisor minimum magnitude is 2^63 and must survive sign stripping
    static_assert(signed_signed_sut(signed_wide_t{min<int64_t>()}, min<int64_t>(), rounding_mode) == int64_t{1});

    // dividend minimum magnitude is 2^127 and must survive sign stripping
    static_assert(signed_unsigned_sut(min<signed_wide_t>(), uint64_t{1}, rounding_mode) == min<int64_t>());

    // large positive result saturates
    static_assert(signed_unsigned_sut(max<signed_wide_t>(), uint64_t{1}, rounding_mode) == max<int64_t>());

    // unsigned wide dividend with negative divisor saturates negative
    static_assert(unsigned_signed_sut(max<wide_t>(), int64_t{-1}, rounding_mode) == min<int64_t>());

    // unsigned wide dividend with positive divisor saturates positive
    static_assert(unsigned_signed_sut(max<wide_t>(), int64_t{1}, rounding_mode) == max<int64_t>());
};

// --------------------------------------------------------------------------------------------------------------------
// Unsigned
// --------------------------------------------------------------------------------------------------------------------

template <typename narrow_t> struct unsigned_test_t
{
    using wide_divider_t = tracking_wide_divider_t<narrow_t>;
    using wide_t = wide_divider_t::wide_t;

    static constexpr auto rounding_mode = tracking_rounding_mode_t<narrow_t>{.id = expected_tracking_rounding_mode_id};
    static constexpr auto max_narrow = max<narrow_t>();

    constexpr auto test_common(auto const& sut) const noexcept -> void
    {
        // (5 << 3) / 2 = 40 / 2 = 20
        static_assert(sut(narrow_t{5}, narrow_t{2}, rounding_mode) == narrow_t{20});

        // zero dividend
        static_assert(sut(narrow_t{0}, narrow_t{5}, rounding_mode) == narrow_t{0});
    }

    constexpr auto test_saturating() const noexcept -> void
    {
        using sut_t = shifted_int_divider_t<wide_divider_t, default_shift, narrow_t, narrow_t, narrow_t, true>;

        constexpr auto sut = sut_t{};

        test_common(sut);

        // saturation: (max << 3) / 1 would overflow narrow bounds, so must clamp
        static_assert(sut(max_narrow, narrow_t{1}, rounding_mode) == max_narrow);

        // zero divisor
        static_assert(sut(narrow_t{1}, narrow_t{0}, rounding_mode) == max_narrow);
        static_assert(sut(narrow_t{0}, narrow_t{0}, rounding_mode) == narrow_t{0});
    }

    constexpr auto test_truncating() const noexcept -> void
    {
        using sut_t = shifted_int_divider_t<wide_divider_t, default_shift, narrow_t, narrow_t, narrow_t, false>;

        constexpr auto sut = sut_t{};

        test_common(sut);

        // truncation: non-saturating adapters truncate instead of clamping
        constexpr auto truncated = static_cast<narrow_t>((static_cast<wide_t>(max_narrow) << default_shift) / 1);
        static_assert(sut(max_narrow, narrow_t{1}, rounding_mode) == truncated);
    }
};

template struct unsigned_test_t<uint8_t>;
template struct unsigned_test_t<uint16_t>;
template struct unsigned_test_t<uint32_t>;

// --------------------------------------------------------------------------------------------------------------------
// Signed
// --------------------------------------------------------------------------------------------------------------------

template <typename narrow_t> struct signed_test_t
{
    using unsigned_t = make_unsigned_t<narrow_t>;
    using rounding_mode_t = tracking_rounding_mode_t<unsigned_t>;
    using wide_divider_t = tracking_wide_divider_t<unsigned_t>;
    using wide_t = wide_divider_t::wide_t;

    static constexpr auto rounding_mode = rounding_mode_t{.id = expected_tracking_rounding_mode_id};

    static constexpr auto max_narrow = max<narrow_t>();
    static constexpr auto min_narrow = min<narrow_t>();

    constexpr auto test_common(auto const& sut) const noexcept -> void
    {
        // sign by quadrant, (6 << 3) / 2 = 48 / 2 = 24
        static_assert(sut(narrow_t{6}, narrow_t{2}, rounding_mode) == narrow_t{24});
        static_assert(sut(narrow_t{-6}, narrow_t{2}, rounding_mode) == narrow_t{-24});
        static_assert(sut(narrow_t{6}, narrow_t{-2}, rounding_mode) == narrow_t{-24});
        static_assert(sut(narrow_t{-6}, narrow_t{-2}, rounding_mode) == narrow_t{24});

        // zero dividend
        static_assert(sut(narrow_t{0}, narrow_t{5}, rounding_mode) == narrow_t{0});
        static_assert(sut(narrow_t{0}, narrow_t{-5}, rounding_mode) == narrow_t{0});
    }

    constexpr auto test_saturating() const noexcept -> void
    {
        using sut_t = shifted_int_divider_t<wide_divider_t, default_shift, narrow_t, narrow_t, narrow_t, true>;

        constexpr auto sut = sut_t{};

        test_common(sut);

        // positive saturation
        static_assert(sut(max_narrow, narrow_t{1}, rounding_mode) == max_narrow);

        // negative asymmetric saturation: (min << 3) / 8 should stay min without overflowing the positive boundary
        static_assert(sut(min_narrow, narrow_t{8}, rounding_mode) == min_narrow);

        // negative saturation (min << 3 / 1 clamps to min)
        static_assert(sut(min_narrow, narrow_t{1}, rounding_mode) == min_narrow);

        // zero divisor
        static_assert(sut(narrow_t{1}, narrow_t{0}, rounding_mode) == max_narrow);
        static_assert(sut(narrow_t{-1}, narrow_t{0}, rounding_mode) == min_narrow);
        static_assert(sut(narrow_t{0}, narrow_t{0}, rounding_mode) == narrow_t{0});
    }

    constexpr auto test_truncating() noexcept -> void
    {
        using sut_t = shifted_int_divider_t<wide_divider_t, default_shift, narrow_t, narrow_t, narrow_t, false>;

        constexpr auto sut = sut_t{};

        test_common(sut);

        // Exact minimum magnitude shifted by 3 is a multiple of the output modulus, so truncation produces zero.
        static_assert(sut(min_narrow, narrow_t{1}, rounding_mode) == narrow_t{0});

        // One above minimum has magnitude:
        //
        //     2^(N-1) - 1
        //
        // After shifting by 3:
        //
        //     2^(N+2) - 8
        //
        // Truncating that magnitude to N bits produces 2^N - 8. Restoring the negative sign modulo N bits produces +8.
        constexpr auto negative_overflow = static_cast<narrow_t>(min_narrow + narrow_t{1});
        static_assert(sut(negative_overflow, narrow_t{1}, rounding_mode) == narrow_t{8});
    }
};

template struct signed_test_t<int8_t>;
template struct signed_test_t<int16_t>;
template struct signed_test_t<int32_t>;

} // namespace
} // namespace crv::division
