// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "segment_locator.hpp"
#include <crv/math/integer.hpp>
#include <crv/math/limits.hpp>
#include <crv/math/spline/fixed/segment_locator_builder.hpp>
#include <crv/math/spline/fixed/segment_locator_test.hpp>
#include <crv/test/test.hpp>
#include <algorithm>

namespace crv::spline::fixed_point::segment_locator {
namespace {

using location_t = int_t;

// --------------------------------------------------------------------------------------------------------------------
// sweep tests
// --------------------------------------------------------------------------------------------------------------------

namespace sweep_tests {

template <int_t t_depth> struct verify_locator_sweep
{
    using traits_t = traits_t<location_t, t_depth>;
    static constexpr auto total_key_count = traits_t::total_key_count;
    using keys_t = std::array<location_t, total_key_count>;

    // reference implementation: count of keys <= x is the segment index; last such key is x_left
    constexpr auto expected_result(keys_t const& keys, location_t x) -> result_t<location_t>
    {
        auto const bound = std::upper_bound(keys.begin(), keys.end(), x);
        auto const index = int_cast<int_t>(bound - keys.begin());
        auto const x_left = (bound == keys.begin()) ? location_t{0} : *(bound - 1);
        return {.index = index, .x_left = x_left};
    }

    constexpr auto sweep(keys_t const& keys) -> bool
    {
        using builder_t = builder_t<traits_t>;
        auto const locator = locator_t<traits_t>{builder_t{}(keys)};

        // sweep one below and n above expected range of keys
        auto prev_index = int_t{0};
        auto const x_max = keys.back() + location_t{5};
        for (auto x = location_t{0}; x <= x_max; ++x)
        {
            auto const result = locator(x);

            if (result != expected_result(keys, x)) return false;
            if (result.x_left > x) return false; // x_left <= x
            if (result.index < prev_index) return false; // monotonic in x
            prev_index = result.index;
        }

        return true;
    }

    constexpr auto test() -> bool
    {
        return sweep(test::make_sequential_keys<location_t, total_key_count>())
            && sweep(test::make_strided_keys<location_t, total_key_count>(3, 5));
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

using traits_t = traits_t<location_t, 3>;
using sut_t = locator_t<traits_t>;

struct tracking_prefetcher_t
{
    mutable bool node_written = false;
    mutable traits_t::data_t::node_t actual_node;

    constexpr auto prefetch(traits_t::data_t::node_t const* node) const noexcept
    {
        node_written = true;
        actual_node = *node;
    }
};

constexpr auto test_prefetcher() noexcept -> bool
{
    auto const prefetcher = tracking_prefetcher_t{};

    using builder_t = builder_t<traits_t>;
    auto const sut = sut_t{builder_t{}(test::make_sequential_keys<location_t, traits_t::total_key_count>())};

    sut.prefetch(prefetcher);

    return prefetcher.node_written && prefetcher.actual_node.keys == traits_t::data_t::node_t::keys_t{{16, 32, 48}};
}
static_assert(test_prefetcher());

} // namespace prefetch_tests

} // namespace
} // namespace crv::spline::fixed_point::segment_locator
