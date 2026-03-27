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

constexpr auto default_shift                      = 3;
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

// fake divider that tracks the rounding mode id
struct tracking_wide_divider_t
{
    template <unsigned_integral narrow_t>
    constexpr auto operator()(wider_t<narrow_t> dividend, narrow_t divisor,
                              tracking_rounding_mode_t<make_unsigned_t<narrow_t>> rm) const noexcept
        -> wider_t<narrow_t>
    {
        using wide_t = wider_t<narrow_t>;

        // verify correct rounding mode instance was forwarded
        if (rm.id != expected_tracking_rounding_mode_id) return static_cast<wide_t>(poisoned_rounded_sentinel);

        auto const actual = dividend / divisor;
        assert(actual != poisoned_rounded_sentinel); // ensure sure sentinel doesn't come up naturally
        return int_cast<wide_t>(actual);
    }
};

// ====================================================================================================================
// Tests
// ====================================================================================================================

// test that empty base optimization is enabled
using ebo_sut_t = shifted_int_divider_t<tracking_wide_divider_t, default_shift, int_t, int_t, int_t, true>;
static_assert(sizeof(ebo_sut_t) == 1, "shifted_int_divider_t should not add overhead for empty dividers");

// --------------------------------------------------------------------------------------------------------------------
// Heterogeneous
// --------------------------------------------------------------------------------------------------------------------

struct heterogeneous_test_t
{
    static constexpr auto rounding_mode = tracking_rounding_mode_t<uint32_t>{.id = expected_tracking_rounding_mode_id};

    constexpr auto test() const noexcept -> void
    {
        using sut_t = shifted_int_divider_t<tracking_wide_divider_t, default_shift, int16_t, uint32_t, int8_t, true>;
        constexpr auto sut = sut_t{};

        // fits: (100 << 3) / -2 = 800 / -2 = -400
        static_assert(sut(uint32_t{100}, int8_t{-2}, rounding_mode) == int16_t{-400});

        // bounds test: clamps to min<int16_t>
        static_assert(sut(uint32_t{10000}, int8_t{-1}, rounding_mode) == min<int16_t>());

        // bounds test: clamps to max<int16_t>
        static_assert(sut(uint32_t{10000}, int8_t{1}, rounding_mode) == max<int16_t>());

        // Unsigned Output boundary test
        using unsigned_out_sut_t
            = shifted_int_divider_t<tracking_wide_divider_t, default_shift, uint16_t, int32_t, int32_t, true>;
        constexpr auto u_sut = unsigned_out_sut_t{};

        // negative mathematical result correctly clamped to 0 for unsigned output type
        static_assert(u_sut(int32_t{100}, int32_t{-2}, rounding_mode) == uint16_t{0});
    }
};

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
        // (5 << 3) / 2 = 40 / 2 = 20
        static_assert(sut(narrow_t{5}, narrow_t{2}, rounding_mode) == narrow_t{20});

        // zero dividend
        static_assert(sut(narrow_t{0}, narrow_t{5}, rounding_mode) == narrow_t{0});
    }

    constexpr auto test_saturating() const noexcept -> void
    {
        using sut_t = shifted_int_divider_t<tracking_wide_divider_t, default_shift, narrow_t, narrow_t, narrow_t, true>;

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
        using sut_t
            = shifted_int_divider_t<tracking_wide_divider_t, default_shift, narrow_t, narrow_t, narrow_t, false>;

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
    using wide_t     = wider_t<unsigned_t>;

    static constexpr auto rounding_mode
        = tracking_rounding_mode_t<unsigned_t>{.id = expected_tracking_rounding_mode_id};

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
        using sut_t = shifted_int_divider_t<tracking_wide_divider_t, default_shift, narrow_t, narrow_t, narrow_t, true>;

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
        using sut_t
            = shifted_int_divider_t<tracking_wide_divider_t, default_shift, narrow_t, narrow_t, narrow_t, false>;

        constexpr auto sut = sut_t{};

        test_common(sut);

        // truncation on negative overflow
        constexpr auto abs_min_val = static_cast<wide_t>(max_narrow) + 1;
        constexpr auto truncated   = (abs_min_val << default_shift) / 1;
        constexpr auto magnitude   = static_cast<unsigned_t>(truncated);
        static_assert(sut(min_narrow, narrow_t{1}, rounding_mode) == static_cast<narrow_t>(-magnitude));
    }
};

template struct signed_test_t<int8_t>;
template struct signed_test_t<int16_t>;
template struct signed_test_t<int32_t>;

} // namespace
} // namespace crv::division
