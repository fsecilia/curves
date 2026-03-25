// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "saturating_divider.hpp"
#include <crv/math/division/result.hpp>
#include <crv/test/test.hpp>

namespace crv::division {
namespace {

// ====================================================================================================================
// Test Doubles
// ====================================================================================================================

constexpr auto expected_tracking_rounding_mode_id = 8675309;
constexpr auto poisoned_rounded_sentinel          = 0xBAD;

// dummy rounding mode that carries a token to prove the adapter forwards it properly
template <unsigned_integral narrow_t> struct tracking_rounding_mode_t
{
    using wide_t = wider_t<narrow_t>;

    int_t id = 0;

    constexpr auto bias(wide_t dividend, narrow_t) const noexcept -> wide_t;
    constexpr auto carry(wide_t quotient, narrow_t, narrow_t) const noexcept -> wide_t;
};

// fake divider that simulates a preshift by scaling the dividend by 10
struct scale_by_ten_divider_t
{
    template <unsigned_integral narrow_t>
    constexpr auto operator()(narrow_t dividend, narrow_t divisor,
                              tracking_rounding_mode_t<make_unsigned_t<narrow_t>> rm) const noexcept
        -> wider_t<narrow_t>
    {
        using wide_t = wider_t<narrow_t>;

        // verify correct rounding mode instance was forwarded
        if (rm.id != expected_tracking_rounding_mode_id) return static_cast<wide_t>(poisoned_rounded_sentinel);

        return (static_cast<wide_t>(dividend) * 10) / divisor;
    }
};

// ====================================================================================================================
// Tests
// ====================================================================================================================

// test that empty base optimization is enabled
using ebo_sut_t = saturating_divider_t<scale_by_ten_divider_t, true>;
static_assert(sizeof(ebo_sut_t) == 1, "saturating_divider_t should not add overhead for empty dividers");

// --------------------------------------------------------------------------------------------------------------------
// Unsigned
// --------------------------------------------------------------------------------------------------------------------

template <typename narrow_t> struct unsigned_test_t
{
    using wide_t                        = wider_t<narrow_t>;
    static constexpr auto rounding_mode = tracking_rounding_mode_t<narrow_t>{.id = expected_tracking_rounding_mode_id};
    static constexpr auto max_narrow    = max<narrow_t>();

    constexpr auto test_common(auto const& sut) const noexcept -> void
    {
        // (5*10)/2 = 25
        static_assert(sut(narrow_t{5}, narrow_t{2}, rounding_mode) == narrow_t{25});

        // zero dividend
        static_assert(sut(narrow_t{0}, narrow_t{5}, rounding_mode) == narrow_t{0});
    }

    constexpr auto test_saturating() const noexcept -> void
    {
        using sut_t = saturating_divider_t<scale_by_ten_divider_t, true>;

        constexpr auto sut = sut_t{};

        test_common(sut);

        // saturation: (max*10)/1 would overflow narrow bounds, so must clamp
        static_assert(sut(max_narrow, narrow_t{1}, rounding_mode) == max_narrow);

        // zero divisor
        static_assert(sut(narrow_t{1}, narrow_t{0}, rounding_mode) == max_narrow);
        static_assert(sut(narrow_t{0}, narrow_t{0}, rounding_mode) == narrow_t{0});
    }

    constexpr auto test_truncating() const noexcept -> void
    {
        using sut_t = saturating_divider_t<scale_by_ten_divider_t, false>;

        constexpr auto sut = sut_t{};

        test_common(sut);

        // truncation: non-saturating adapters truncate instead of clamping
        constexpr auto truncated = static_cast<narrow_t>((static_cast<wide_t>(max_narrow) * 10) / 1);
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
    using wide_t     = wider_t<unsigned_t>;

    static constexpr auto rounding_mode
        = tracking_rounding_mode_t<unsigned_t>{.id = expected_tracking_rounding_mode_id};

    static constexpr auto max_narrow = max<narrow_t>();
    static constexpr auto min_narrow = min<narrow_t>();

    constexpr auto test_common(auto const& sut) const noexcept -> void
    {
        // sign by quadrant, 6 * 10 / 2 = 30
        static_assert(sut(narrow_t{6}, narrow_t{2}, rounding_mode) == narrow_t{30});
        static_assert(sut(narrow_t{-6}, narrow_t{2}, rounding_mode) == narrow_t{-30});
        static_assert(sut(narrow_t{6}, narrow_t{-2}, rounding_mode) == narrow_t{-30});
        static_assert(sut(narrow_t{-6}, narrow_t{-2}, rounding_mode) == narrow_t{30});

        // zero dividend
        static_assert(sut(narrow_t{0}, narrow_t{5}, rounding_mode) == narrow_t{0});
        static_assert(sut(narrow_t{0}, narrow_t{-5}, rounding_mode) == narrow_t{0});
    }

    constexpr auto test_saturating() const noexcept -> void
    {
        using sut_t = saturating_divider_t<scale_by_ten_divider_t, true>;

        constexpr auto sut = sut_t{};

        test_common(sut);

        // positive saturation
        static_assert(sut(max_narrow, narrow_t{1}, rounding_mode) == max_narrow);

        // negative asymmetric saturation: (min*10)/10 should stay min without overflowing the positive boundary
        static_assert(sut(min_narrow, narrow_t{10}, rounding_mode) == min_narrow);

        // negative saturation (min*10/1 clamps to min)
        static_assert(sut(min_narrow, narrow_t{1}, rounding_mode) == min_narrow);

        // zero divisor
        static_assert(sut(narrow_t{1}, narrow_t{0}, rounding_mode) == max_narrow);
        static_assert(sut(narrow_t{-1}, narrow_t{0}, rounding_mode) == min_narrow);
        static_assert(sut(narrow_t{0}, narrow_t{0}, rounding_mode) == narrow_t{0});
    }

    constexpr auto test_truncating() noexcept -> void
    {
        using sut_t = saturating_divider_t<scale_by_ten_divider_t, false>;

        constexpr auto sut = sut_t{};

        test_common(sut);

        // truncation on deep negative overflow
        constexpr auto abs_min_val = static_cast<wide_t>(max_narrow) + 1;
        constexpr auto truncated   = (abs_min_val * 10) / 1;
        constexpr auto magnitude   = static_cast<unsigned_t>(truncated);
        static_assert(sut(min_narrow, narrow_t{1}, rounding_mode) == static_cast<narrow_t>(-magnitude));
    }
};

template struct signed_test_t<int8_t>;
template struct signed_test_t<int16_t>;
template struct signed_test_t<int32_t>;

} // namespace
} // namespace crv::division
