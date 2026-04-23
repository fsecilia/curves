// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "segment_locator.hpp"
#include <crv/math/integer.hpp>
#include <crv/math/limits.hpp>
#include <crv/test/test.hpp>
#include <algorithm>

namespace crv::spline {
namespace {

using location_t = int_t;

template <typename sorted_keys_t> constexpr auto make_sequential_keys() -> sorted_keys_t
{
    sorted_keys_t result;
    for (auto i = 0u; i < std::size(result); ++i) result[i] = location_t{i + 1};
    return result;
}

template <typename sorted_keys_t> constexpr auto make_strided_keys(int_t offset, int_t stride) -> sorted_keys_t
{
    sorted_keys_t result;
    for (auto i = 0u; i < std::size(result); ++i) result[i] = location_t{offset + i * stride};
    return result;
}

// ====================================================================================================================
// offsets_t
// ====================================================================================================================

namespace offsets_tests {

template <int_t depth_max> using offsets_by_depth_t = segment_locator_t<location_t, depth_max>::row_offsets_t;

// (4^n - 1)/3
static_assert(offsets_by_depth_t<0>{}.base_index == std::array<int_t, 0>{});
static_assert(offsets_by_depth_t<1>{}.base_index == std::array<int_t, 1>{0});
static_assert(offsets_by_depth_t<2>{}.base_index == std::array<int_t, 2>{0, 1});
static_assert(offsets_by_depth_t<3>{}.base_index == std::array<int_t, 3>{0, 1, 5});
static_assert(offsets_by_depth_t<4>{}.base_index == std::array<int_t, 4>{0, 1, 5, 21});
static_assert(offsets_by_depth_t<5>{}.base_index == std::array<int_t, 5>{0, 1, 5, 21, 85});
static_assert(offsets_by_depth_t<6>{}.base_index == std::array<int_t, 6>{0, 1, 5, 21, 85, 341});

} // namespace offsets_tests

// --------------------------------------------------------------------------------------------------------------------
// sweep tests
// --------------------------------------------------------------------------------------------------------------------

namespace sweep_tests {

template <int_t depth_max> struct verify_locator_sweep
{
    using sut_t = segment_locator_t<location_t, depth_max>;
    static constexpr auto total_key_count = sut_t::total_key_count;
    using sorted_keys_t = std::array<location_t, total_key_count>;

    // reference implementation: count of keys <= x is the segment index; last such key is origin
    constexpr auto expected_result(sorted_keys_t const& keys, location_t x) -> sut_t::result_t
    {
        auto const bound = std::upper_bound(keys.begin(), keys.end(), x);
        auto const index = int_cast<int_t>(bound - keys.begin());
        auto const origin = (bound == keys.begin()) ? location_t{0} : *(bound - 1);
        return {.index = index, .origin = origin};
    }

    constexpr auto sweep(sorted_keys_t const& keys) -> bool
    {
        auto const locator = sut_t{keys};

        // sweep one below and n above expected range of keys
        auto prev_index = int_t{0};
        auto const x_max = keys.back() + location_t{5};
        for (auto x = location_t{0}; x <= x_max; ++x)
        {
            auto const result = locator(x);

            if (result != expected_result(keys, x)) return false;
            if (result.origin > x) return false; // origin <= x
            if (result.index < prev_index) return false; // monotonic in x
            prev_index = result.index;
        }

        return true;
    }

    constexpr auto test() -> bool
    {
        return sweep(make_sequential_keys<sorted_keys_t>()) && sweep(make_strided_keys<sorted_keys_t>(3, 5));
    }
};

static_assert(verify_locator_sweep<1>{}.test());
static_assert(verify_locator_sweep<2>{}.test());
static_assert(verify_locator_sweep<3>{}.test());
static_assert(verify_locator_sweep<4>{}.test());

} // namespace sweep_tests

// --------------------------------------------------------------------------------------------------------------------
// prefetch
// --------------------------------------------------------------------------------------------------------------------

namespace prefetch_tests {

using sut_t = segment_locator_t<location_t, 3>;
using node_keys_t = sut_t::node_keys_t;

static constexpr auto total_key_count = sut_t::total_key_count;
using sorted_keys_t = std::array<location_t, total_key_count>;

struct tracking_prefetcher_t
{
    mutable bool node_written = false;
    mutable sut_t::node_t actual_node;

    constexpr auto prefetch(sut_t::node_t const* node) const noexcept
    {
        node_written = true;
        actual_node = *node;
    }
};

constexpr auto test_prefetcher() noexcept -> bool
{
    auto const prefetcher = tracking_prefetcher_t{};
    auto const sut = sut_t{make_sequential_keys<sorted_keys_t>()};

    sut.prefetch(prefetcher);

    return prefetcher.node_written && prefetcher.actual_node.keys == node_keys_t{{16, 32, 48}};
}
static_assert(test_prefetcher());

} // namespace prefetch_tests

// --------------------------------------------------------------------------------------------------------------------
// is_valid tests
// --------------------------------------------------------------------------------------------------------------------

namespace is_valid_tests {

using sut_t = segment_locator_t<location_t, 2>;
static constexpr auto total_key_count = sut_t::total_key_count;
using sorted_keys_t = std::array<location_t, total_key_count>;

// valid bounds and midpoint
static_assert(sut_t{make_sequential_keys<sorted_keys_t>()}.is_valid(1));
static_assert(sut_t{make_sequential_keys<sorted_keys_t>()}.is_valid(sut_t::max_segment_count / 2));
static_assert(sut_t{make_sequential_keys<sorted_keys_t>()}.is_valid(sut_t::max_segment_count));

// out of bounds: zero, negative, and strictly greater than max
static_assert(!sut_t{make_sequential_keys<sorted_keys_t>()}.is_valid(0));
static_assert(!sut_t{make_sequential_keys<sorted_keys_t>()}.is_valid(-1));
static_assert(!sut_t{make_sequential_keys<sorted_keys_t>()}.is_valid(sut_t::max_segment_count + 1));

constexpr auto test_duplicate_keys() -> bool
{
    auto keys = make_sequential_keys<sorted_keys_t>();

    // create a duplicate adjacent key
    keys[5] = keys[4];

    return !sut_t{keys}.is_valid(sut_t::max_segment_count);
}
static_assert(test_duplicate_keys());

constexpr auto test_out_of_order_keys() -> bool
{
    auto keys = make_sequential_keys<sorted_keys_t>();

    // break monotonic sort
    keys[5] = keys[4] - location_t{1};

    return !sut_t{keys}.is_valid(sut_t::max_segment_count);
}
static_assert(test_out_of_order_keys());

constexpr auto test_minimum_bound_key() -> bool
{
    auto keys = make_sequential_keys<sorted_keys_t>();

    // min is not a valid location for a key; it must come before all keys
    keys.front() = min<location_t>();

    return !sut_t{keys}.is_valid(sut_t::max_segment_count);
}
static_assert(test_minimum_bound_key());

constexpr auto test_duplicate_at_min_boundary() -> bool
{
    auto keys = make_sequential_keys<sorted_keys_t>();
    keys[1] = keys.front();

    return !sut_t{keys}.is_valid(sut_t::max_segment_count);
}
static_assert(test_duplicate_at_min_boundary());

constexpr auto test_duplicate_at_max_boundary() -> bool
{
    auto keys = make_sequential_keys<sorted_keys_t>();
    keys.back() = keys[keys.size() - 2];

    return !sut_t{keys}.is_valid(sut_t::max_segment_count);
}
static_assert(test_duplicate_at_max_boundary());

} // namespace is_valid_tests

} // namespace
} // namespace crv::spline
