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

// ====================================================================================================================
// row_offsets_t
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

// ====================================================================================================================
// segment_locator_t
// ====================================================================================================================

// --------------------------------------------------------------------------------------------------------------------
// depth 0 (degenerate / single segment)
// --------------------------------------------------------------------------------------------------------------------

namespace depth_zero_tests {

using sut_t = segment_locator_t<location_t, 0>;

constexpr auto empty_keys = std::array<location_t, 0>{};
constexpr auto sut = sut_t{empty_keys, 5};

// validation
static_assert(sut.is_valid(1));
static_assert(!sut.is_valid(0));

// bypass tree descent
static_assert(sut.locate(10) == sut_t::result_t{0, 0});
static_assert(sut.locate(100) == sut_t::result_t{0, 0});

} // namespace depth_zero_tests

// --------------------------------------------------------------------------------------------------------------------
// query boundaries
// --------------------------------------------------------------------------------------------------------------------

namespace boundary_query_tests {

using sut_t = segment_locator_t<location_t, 1>;
constexpr auto keys = std::array<location_t, 3>{10, 20, 30};
constexpr auto sut = sut_t{keys, 40};

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

// --------------------------------------------------------------------------------------------------------------------
// prefetch
// --------------------------------------------------------------------------------------------------------------------

namespace prefetch_tests {

using sut_t = segment_locator_t<location_t, 3>;
using node_keys_t = sut_t::node_keys_t;

struct tracking_prefetcher_t
{
    mutable sut_t::node_t actual_node;
    mutable int_t actual_cache_line_count = 0;

    constexpr auto prefetch(sut_t::node_t const* node, int_t cache_line_count) const noexcept -> void
    {
        actual_node = *node;
        actual_cache_line_count = cache_line_count;
    }
};

constexpr auto test_prefetcher() noexcept -> bool
{
    auto const prefetcher = tracking_prefetcher_t{};

    auto keys = std::array<location_t, sut_t::total_key_count>{};
    for (auto i = 0u; i < keys.size(); ++i) keys[i] = location_t{i + 1};

    auto const sut = sut_t{keys, 0};
    sut.prefetch(prefetcher);

    return prefetcher.actual_cache_line_count == 2 && prefetcher.actual_node.keys == node_keys_t{{16, 32, 48}};
}
static_assert(test_prefetcher());

} // namespace prefetch_tests

// --------------------------------------------------------------------------------------------------------------------
// is_valid
// --------------------------------------------------------------------------------------------------------------------

namespace is_valid_tests {

using sut_t = segment_locator_t<location_t, 1>;
using segments_t = std::array<location_t, 3>;

// valid baseline
static_assert(sut_t{segments_t{10, 20, 30}, 40}.is_valid(4));

// duplicate first pair
static_assert(!sut_t{segments_t{10, 10, 20}, 40}.is_valid(4));

// duplicate last pair
static_assert(!sut_t{segments_t{10, 20, 20}, 40}.is_valid(4));

// out of order
static_assert(!sut_t{segments_t{10, 30, 20}, 40}.is_valid(4));

// min bound key
static_assert(!sut_t{segments_t{min<location_t>(), 20, 30}, 40}.is_valid(4));

// padding validation with fewer than max segments, all padding >= x_max
static_assert(sut_t{segments_t{10, 50, 60}, 20}.is_valid(2));

// padding validation with fewer than max segments
static_assert(!sut_t{segments_t{10, 15, 60}, 20}.is_valid(2));

// padding validation not monotonic
static_assert(!sut_t{segments_t{10, 60, 50}, 20}.is_valid(2));

} // namespace is_valid_tests

// --------------------------------------------------------------------------------------------------------------------
// sweep tests
// --------------------------------------------------------------------------------------------------------------------

namespace sweep_tests {

// reference implementation: count of keys <= x is the segment index; last such key is origin
template <int_t depth_max> constexpr auto expected_result(std::span<location_t const> keys, location_t x)
{
    using sut_t = segment_locator_t<location_t, depth_max>;
    auto const bound = std::upper_bound(keys.begin(), keys.end(), x);
    auto const index = static_cast<int_t>(bound - keys.begin());
    auto const origin = (bound == keys.begin()) ? location_t{0} : *(bound - 1);
    return typename sut_t::result_t{.index = index, .origin = origin};
}

template <int_t depth_max> constexpr auto test_sweep(int_t offset, int_t stride) -> bool
{
    using sut_t = segment_locator_t<location_t, depth_max>;
    std::array<location_t, sut_t::total_key_count> keys{};

    // generate strided keys
    for (auto i = 0u; i < keys.size(); ++i) { keys[i] = location_t{offset + static_cast<int_t>(i) * stride}; }

    auto const x_max = location_t{offset + static_cast<int_t>(keys.size()) * stride};
    auto const sut = sut_t{keys, x_max};

    // sweep one below and n above expected range of keys
    auto prev_index = int_t{0};
    for (auto x = location_t{0}; x <= x_max; ++x)
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
