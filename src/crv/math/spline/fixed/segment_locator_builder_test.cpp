// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "segment_locator_builder.hpp"
#include <crv/math/limits.hpp>
#include <crv/math/spline/fixed/segment_locator_test.hpp>
#include <crv/test/test.hpp>

namespace crv::spline::fixed_point::segment_locator {
namespace {

using location_t = int_t;

template <int_t depth_max> using traits_by_depth_t = traits_t<location_t, depth_max>;

// ====================================================================================================================
// offsets_t
// ====================================================================================================================

template <int_t depth_max>
using offsets_by_depth_t = row_offsets_t<depth_max, traits_by_depth_t<depth_max>::node_key_count>;

// (4^n - 1)/3
static_assert(offsets_by_depth_t<0>{}.base_index == std::array<int_t, 0>{});
static_assert(offsets_by_depth_t<1>{}.base_index == std::array<int_t, 1>{0});
static_assert(offsets_by_depth_t<2>{}.base_index == std::array<int_t, 2>{0, 1});
static_assert(offsets_by_depth_t<3>{}.base_index == std::array<int_t, 3>{0, 1, 5});
static_assert(offsets_by_depth_t<4>{}.base_index == std::array<int_t, 4>{0, 1, 5, 21});
static_assert(offsets_by_depth_t<5>{}.base_index == std::array<int_t, 5>{0, 1, 5, 21, 85});
static_assert(offsets_by_depth_t<6>{}.base_index == std::array<int_t, 6>{0, 1, 5, 21, 85, 341});

// ====================================================================================================================
// builder_t
// ====================================================================================================================

template <typename traits_t> struct verify_builder_t
{
    using keys_t = std::array<typename traits_t::location_t, traits_t::total_key_count>;

    consteval auto test(traits_t::data_t expected) -> bool
    {
        return test(test::make_sequential_keys<int_t, traits_t::total_key_count>(), expected);
    }

    consteval auto test(keys_t const& keys, traits_t::data_t expected) -> bool
    {
        return builder_t<traits_t>{}(keys) == expected;
    }
};

// --------------------------------------------------------------------------------------------------------------------
// sequential keys
// --------------------------------------------------------------------------------------------------------------------

static_assert(verify_builder_t<traits_t<location_t, 0>>{}.test(traits_t<location_t, 0>::data_t{}));
static_assert(verify_builder_t<traits_t<location_t, 1>>{}.test(traits_t<location_t, 1>::data_t{{{{1, 2, 3}}}}));
static_assert(verify_builder_t<traits_t<location_t, 2>>{}.test(
    traits_t<location_t, 2>::data_t{{{{{4, 8, 12}}, {{1, 2, 3}}, {{5, 6, 7}}, {{9, 10, 11}}, {{13, 14, 15}}}}}));

static_assert(verify_builder_t<traits_t<location_t, 3>>{}.test(traits_t<location_t, 3>::data_t{{{{{16, 32, 48}},

    {{4, 8, 12}}, {{20, 24, 28}}, {{36, 40, 44}}, {{52, 56, 60}},

    {{1, 2, 3}}, {{5, 6, 7}}, {{9, 10, 11}}, {{13, 14, 15}}, {{17, 18, 19}}, {{21, 22, 23}}, {{25, 26, 27}},
    {{29, 30, 31}}, {{33, 34, 35}}, {{37, 38, 39}}, {{41, 42, 43}}, {{45, 46, 47}}, {{49, 50, 51}}, {{53, 54, 55}},
    {{57, 58, 59}}, {{61, 62, 63}}}}}));

// --------------------------------------------------------------------------------------------------------------------
// non-sequential keys
// --------------------------------------------------------------------------------------------------------------------

static_assert(verify_builder_t<traits_t<location_t, 2>>{}.test(
    test::make_strided_keys<location_t, traits_t<location_t, 2>::total_key_count>(3, 5),
    traits_t<location_t, 2>::data_t{{{{{18, 38, 58}}, {{3, 8, 13}}, {{23, 28, 33}}, {{43, 48, 53}}, {{63, 68, 73}}}}}));

} // namespace
} // namespace crv::spline::fixed_point::segment_locator
