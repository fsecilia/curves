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

using x_t = int_t;

// --------------------------------------------------------------------------------------------------------------------
// row_offsets_t
// --------------------------------------------------------------------------------------------------------------------

namespace offsets_tests {

template <int_t depth_max> using offsets_by_depth_t = segment_locator_t<x_t, depth_max>::row_offsets_t;

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
// segment_locator_t
// --------------------------------------------------------------------------------------------------------------------

//
// depth 0 (degenerate / single segment)
//

namespace depth_zero_tests {

using sut_t = segment_locator_t<x_t, 0>;

constexpr auto empty_keys = std::array<x_t, 0>{};

// validation
static_assert(sut_t{empty_keys, 5, 1}.is_valid());
static_assert(!sut_t{empty_keys, 5, 0}.is_valid());

// bypass tree descent
constexpr auto sut = sut_t{empty_keys, 5, 1};
static_assert(sut.locate(10) == sut_t::result_t{0, 0});
static_assert(sut.locate(100) == sut_t::result_t{0, 0});

} // namespace depth_zero_tests

//
// propeties
//

namespace property_tests {

using sut_t = segment_locator_t<x_t, 1>;
constexpr auto keys = std::array<x_t, 3>{10, 20, 30};
constexpr auto x_max = x_t{40};

static_assert(sut_t{keys, x_max, sut_t::max_segment_count}.segment_count() == sut_t::max_segment_count);
static_assert(sut_t{keys, x_max, sut_t::max_segment_count}.x_max() == x_max);

} // namespace property_tests

//
// query boundaries
//

namespace boundary_query_tests {

using sut_t = segment_locator_t<x_t, 1>;
constexpr auto keys = std::array<x_t, 3>{10, 20, 30};
constexpr auto sut = sut_t{keys, 40, sut_t::max_segment_count};

// smallest value
static_assert(sut.locate(0) == sut_t::result_t{0, 0});

// before the first key
static_assert(sut.locate(9) == sut_t::result_t{0, 0});

// first key exact
static_assert(sut.locate(10) == sut_t::result_t{1, 10});

// second key exact
static_assert(sut.locate(20) == sut_t::result_t{2, 20});

// just before x_max
static_assert(sut.locate(39) == sut_t::result_t{3, 30});

// x_max
static_assert(sut.locate(40) == sut_t::result_t{3, 30});

// after x_max
static_assert(sut.locate(100) == sut_t::result_t{3, 30});

} // namespace boundary_query_tests

//
// prefetch
//

namespace prefetch_tests {

using sut_t = segment_locator_t<x_t, 3>;
using node_keys_t = sut_t::node_keys_t;

struct tracking_prefetcher_t
{
    mutable sut_t::node_t actual_node;
    mutable int_t actual_cache_line_count = 0;

    constexpr auto prefetch(sut_t::node_t const* node) const noexcept -> void
    {
        actual_node = *node;
        actual_cache_line_count = 1;
    }

};

constexpr auto test_leaf_prefetch() noexcept -> bool
{
    auto const prefetcher = tracking_prefetcher_t{};

    auto keys = std::array<x_t, sut_t::total_key_count>{};
    for (auto i = 0u; i < keys.size(); ++i) keys[i] = x_t{i + 1};

    auto const sut = sut_t{keys, 64, sut_t::max_segment_count};

    auto const hint = sut_t::hint_t{.segment_index = 9};
    sut.prefetch(hint, prefetcher);

    return prefetcher.actual_cache_line_count == 1 && prefetcher.actual_node.keys == node_keys_t{{9, 10, 11}};
}
static_assert(test_leaf_prefetch());

} // namespace prefetch_tests

//
// leaf hint
//

namespace leaf_hint_tests {
using sut_t = segment_locator_t<x_t, 2>;
constexpr auto keys = [] {
    auto result = std::array<x_t, sut_t::total_key_count>{};
    for (auto i = 0u; i < result.size(); ++i) result[i] = x_t{10 * static_cast<int_t>(i + 1)};
    return result;
}();
constexpr auto sut = sut_t{keys, 160, sut_t::max_segment_count};

constexpr auto test_leaf_hits_and_fallback() noexcept -> bool
{
    auto hint = sut_t::hint_t{};

    auto const first = sut.locate(25, hint);
    if (first != sut_t::result_t{2, 20}) return false;
    if (hint != sut_t::hint_t{.segment_index = 2, .leaf_origin = 0, .leaf_end = 40}) return false;

    auto const same_leaf = sut.locate(35, hint);
    if (same_leaf != sut_t::result_t{3, 30}) return false;
    if (hint != sut_t::hint_t{.segment_index = 3, .leaf_origin = 0, .leaf_end = 40}) return false;

    auto const next_leaf = sut.locate(45, hint);
    if (next_leaf != sut_t::result_t{4, 40}) return false;
    if (hint != sut_t::hint_t{.segment_index = 4, .leaf_origin = 40, .leaf_end = 80}) return false;

    return true;
}
static_assert(test_leaf_hits_and_fallback());

constexpr auto test_leaf_miss_refreshes_hint() noexcept -> bool
{
    auto hint = sut_t::hint_t{.segment_index = 15, .leaf_origin = 120, .leaf_end = 160};
    auto const location = sut.locate(25, hint);
    return location == sut_t::result_t{2, 20}
    && hint == sut_t::hint_t{.segment_index = 2, .leaf_origin = 0, .leaf_end = 40};
}
static_assert(test_leaf_miss_refreshes_hint());

} // namespace leaf_hint_tests

//
// is_valid
//

namespace is_valid_tests {

using sut_t = segment_locator_t<x_t, 1>;
using segments_t = std::array<x_t, 3>;

// valid baseline
static_assert(sut_t{segments_t{10, 20, 30}, 40, 4}.is_valid());

// negative key
static_assert(!sut_t{segments_t{-10, 20, 30}, 40, 4}.is_valid());

// duplicate first pair
static_assert(!sut_t{segments_t{10, 10, 20}, 40, 4}.is_valid());

// duplicate last pair
static_assert(!sut_t{segments_t{10, 20, 20}, 40, 4}.is_valid());

// out of order
static_assert(!sut_t{segments_t{10, 30, 20}, 40, 4}.is_valid());

// min bound key
static_assert(!sut_t{segments_t{min<x_t>(), 20, 30}, 40, 4}.is_valid());

// padding validation with fewer than max segments, all padding >= x_max
static_assert(sut_t{segments_t{10, 50, 60}, 20, 2}.is_valid());

// padding validation with fewer than max segments
static_assert(!sut_t{segments_t{10, 15, 60}, 20, 2}.is_valid());

// padding validation not monotonic
static_assert(!sut_t{segments_t{10, 60, 50}, 20, 2}.is_valid());

} // namespace is_valid_tests

//
// sweep tests
//

namespace sweep_tests {

// reference implementation: count of keys <= x is the segment index; last such key is origin
template <int_t depth_max> constexpr auto expected_result(std::span<x_t const> keys, x_t x)
{
    using sut_t = segment_locator_t<x_t, depth_max>;
    auto const bound = std::upper_bound(keys.begin(), keys.end(), x);
    auto const index = static_cast<int_t>(bound - keys.begin());
    auto const origin = (bound == keys.begin()) ? x_t{0} : *(bound - 1);
    return typename sut_t::result_t{.index = index, .origin = origin};
}

template <int_t depth_max> constexpr auto test_sweep(int_t offset, int_t stride) -> bool
{
    using sut_t = segment_locator_t<x_t, depth_max>;
    std::array<x_t, sut_t::total_key_count> keys{};

    // generate strided keys
    for (auto i = 0u; i < keys.size(); ++i) { keys[i] = x_t{offset + static_cast<int_t>(i) * stride}; }

    auto const x_max = x_t{offset + static_cast<int_t>(keys.size()) * stride};
    auto const sut = sut_t{keys, x_max, sut_t::max_segment_count};

    // sweep one below and n above expected range of keys
    auto prev_index = int_t{0};
    for (auto x = x_t{0}; x <= x_max; ++x)
    {
        auto const result = sut.locate(x);

        if (result != expected_result<depth_max>(keys, x)) return false;
        if (result.origin > x) return false; // origin <= x
        if (result.index < prev_index) return false; // monotonic in x

        prev_index = result.index;
    }

    return true;
}

static_assert(test_sweep<1>(1, 1));
static_assert(test_sweep<1>(3, 5));
static_assert(test_sweep<2>(1, 1));
static_assert(test_sweep<2>(3, 5));
static_assert(test_sweep<3>(1, 1));
static_assert(test_sweep<4>(1, 1));

} // namespace sweep_tests

} // namespace
} // namespace crv::spline
