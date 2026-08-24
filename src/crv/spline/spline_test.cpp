// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "spline.hpp"
#include <crv/math/limits.hpp>
#include <crv/test/test.hpp>
#include <gmock/gmock.h>

namespace crv::spline {
namespace {

using x_t = int8_t;
using y_t = int16_t;
constexpr auto segment_count = 3;
constexpr auto x_max = x_t{5};

struct segment_t
{
    using x_t = x_t;
    using y_t = y_t;

    y_t base_val;

    constexpr auto operator()(x_t x, x_t x0) const noexcept -> y_t { return static_cast<y_t>(base_val + (x - x0)); }
};

struct extended_tangent_t
{
    y_t base_val;

    constexpr auto operator()(x_t x) const noexcept -> y_t { return static_cast<y_t>(base_val + x); }
};

struct segment_locator_t
{
    static constexpr auto max_segment_count = segment_count;

    struct result_t
    {
        int_t index;
        x_t origin;
        auto operator<=>(result_t const&) const noexcept -> auto = default;
        auto operator==(result_t const&) const noexcept -> bool = default;
    };

    struct hint_t
    {
        int_t segment_index{};
    };

    constexpr auto locate(x_t x, hint_t& hint) const noexcept -> result_t
    {
        x_t const index = x / 2;
        hint.segment_index = index;
        return {.index = static_cast<int_t>(index), .origin = static_cast<x_t>(index * 2)};
    }

    x_t x_max_ = spline::x_max;
    constexpr auto x_max() const noexcept -> x_t { return x_max_; }

    int_t segment_count_ = spline::segment_count;
    constexpr auto segment_count() const noexcept -> int_t { return segment_count_; }

    constexpr auto is_valid() const noexcept -> bool { return true; }
};

using sut_t = spline_t<segment_t, extended_tangent_t, segment_locator_t>;

constexpr auto segments = std::array<segment_t, sut_t::max_segment_count>{{{10}, {20}, {30}}};
constexpr auto extended_tangent = extended_tangent_t{40};
constexpr auto const sut = sut_t{
    .segment_locator = segment_locator_t{x_max, segment_count},
    .segments = segments,
    .extend_final_tangent = extended_tangent,
};

constexpr auto evaluate(x_t x) noexcept -> y_t
{
    auto hint = sut_t::hint_t{};
    return sut.evaluate(x, hint);
}

static_assert(evaluate(0) == 10);
static_assert(evaluate(1) == 11);
static_assert(evaluate(2) == 20);
static_assert(evaluate(3) == 21);
static_assert(evaluate(4) == 30);
static_assert(evaluate(5) == 40);
static_assert(evaluate(6) == 41);
static_assert(evaluate(max<x_t>()) == 162);

struct spline_prefetch_test_t : Test
{
    struct segment_t
    {
        using x_t = x_t;
        using y_t = y_t;

        alignas(32) std::array<std::byte, 32> padding;

        constexpr auto operator()(x_t, x_t) const noexcept -> y_t { return y_t{0}; }
    };

    struct extended_tangent_t
    {
        constexpr auto operator()(x_t) const noexcept -> y_t { return y_t{0}; }
    };

    struct mock_prefetcher_t
    {
        virtual ~mock_prefetcher_t() = default;
        MOCK_METHOD(void, prefetch, (void const* address), (const, noexcept));
    };
    StrictMock<mock_prefetcher_t> mock_prefetcher;

    struct prefetcher_t
    {
        mock_prefetcher_t* mock = nullptr;
        auto prefetch(void const* address) const noexcept -> void { mock->prefetch(address); }
    };
    prefetcher_t prefetcher{&mock_prefetcher};

    struct mock_locator_t
    {
        virtual ~mock_locator_t() = default;
        MOCK_METHOD(void, prefetch_hint, (int_t), (const, noexcept));
    };
    StrictMock<mock_locator_t> mock_locator;

    struct locator_t
    {
        static constexpr auto max_segment_count = segment_count;

        struct result_t
        {
            int_t index;
            x_t origin;
        };

        struct hint_t
        {
            int_t segment_index{};
        };

        mock_locator_t* mock = nullptr;

        auto segment_count() const noexcept -> int_t { return spline::segment_count; }
        auto x_max() const noexcept -> x_t { return spline::x_max; }
        auto is_valid() const noexcept -> bool { return true; }
        auto locate(x_t, hint_t&) const noexcept -> result_t { return {}; }
        auto prefetch(hint_t const& hint, auto const&) const noexcept -> void
        {
            mock->prefetch_hint(hint.segment_index);
        }
    };

    static constexpr auto expected_segment = segment_count - 1;
    static constexpr auto expected_fetch_distance = 2 * sizeof(segment_t);

    using sut_t = spline_t<segment_t, extended_tangent_t, locator_t>;
    sut_t sut{.segment_locator = locator_t{.mock = &mock_locator}};

    typename sut_t::hint_t hint{};
};

TEST_F(spline_prefetch_test_t, prefetches_leaf_then_segment_neighbors)
{
    hint.segment_index = expected_segment;
    void const* prefetched_cache_lines[2];
    {
        auto const seq = InSequence{};
        EXPECT_CALL(mock_locator, prefetch_hint(expected_segment));
        EXPECT_CALL(mock_prefetcher, prefetch(_)).WillOnce(SaveArg<0>(&prefetched_cache_lines[0]));
        EXPECT_CALL(mock_prefetcher, prefetch(_)).WillOnce(SaveArg<0>(&prefetched_cache_lines[1]));
    }
    sut.prefetch(hint, prefetcher);
    auto const actual_distance = static_cast<std::byte const*>(prefetched_cache_lines[1])
        - static_cast<std::byte const*>(prefetched_cache_lines[0]);
    EXPECT_EQ(expected_fetch_distance, actual_distance);
    auto const expected_cache_line_0
        = reinterpret_cast<std::byte const*>(sut.segments.data()) + (expected_segment - 1) * sizeof(segment_t);
    EXPECT_EQ(expected_cache_line_0, static_cast<std::byte const*>(prefetched_cache_lines[0]));
}

// --------------------------------------------------------------------------------------------------------------------
// death tests
// --------------------------------------------------------------------------------------------------------------------

#if defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG

struct spline_death_test_t : Test
{
    static constexpr auto segments = std::array<segment_t, sut_t::max_segment_count>{};
    static constexpr auto extended_tangent = extended_tangent_t{};
};

TEST_F(spline_death_test_t, establish_valid_case)
{
    static_cast<void>(
        sut_t{.segment_locator = segment_locator_t{}, .segments = segments, .extend_final_tangent = extended_tangent});
}

TEST_F(spline_death_test_t, evaluate_catches_negative_x)
{
    auto const spline
        = sut_t{.segment_locator = segment_locator_t{}, .segments = segments, .extend_final_tangent = extended_tangent};
    auto hint = sut_t::hint_t{};
    EXPECT_DEATH(spline.evaluate(x_t{-1}, hint), "input out of bounds");
}

struct spline_death_test_evaluate_malicious_locator_t : spline_death_test_t
{
    struct malicious_locator_t
    {
        using result_t = segment_locator_t::result_t;
        using hint_t = segment_locator_t::hint_t;

        static constexpr auto max_segment_count = segment_count;
        x_t index = 0;
        x_t origin = 0;

        constexpr auto locate(x_t, hint_t&) const noexcept -> result_t
        {
            return {.index = index, .origin = origin};
        }
        constexpr auto x_max() const noexcept -> x_t { return spline::x_max; }
        constexpr auto segment_count() const noexcept -> int_t { return spline::segment_count; }
    };

    using sut_t = spline_t<segment_t, extended_tangent_t, malicious_locator_t>;

    sut_t::hint_t hint{};
};

TEST_F(spline_death_test_evaluate_malicious_locator_t, negative_index)
{
    auto const spline = sut_t{
        .segment_locator = malicious_locator_t{.index = -1},
        .segments = segments,
        .extend_final_tangent = extended_tangent,
    };

    EXPECT_DEATH(spline.evaluate(0, hint), "index out of bounds");
}

TEST_F(spline_death_test_evaluate_malicious_locator_t, oor_index)
{
    auto const spline = sut_t{
        .segment_locator = malicious_locator_t{.index = 127},
        .segments = segments,
        .extend_final_tangent = extended_tangent,
    };

    EXPECT_DEATH(spline.evaluate(0, hint), "index out of bounds");
}

TEST_F(spline_death_test_evaluate_malicious_locator_t, negative_origin)
{
    auto const spline = sut_t{
        .segment_locator = malicious_locator_t{.origin = -1},
        .segments = segments,
        .extend_final_tangent = extended_tangent,
    };

    EXPECT_DEATH(spline.evaluate(0, hint), "origin out of range");
}

TEST_F(spline_death_test_evaluate_malicious_locator_t, oor_origin)
{
    auto const x = x_t{x_max - 2};
    auto const spline = sut_t{
        .segment_locator = malicious_locator_t{.origin = static_cast<x_t>(x + 1)},
        .segments = segments,
        .extend_final_tangent = extended_tangent,
    };

    EXPECT_DEATH(spline.evaluate(x, hint), "origin out of range");
}

#endif // #if defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG

} // namespace
} // namespace crv::spline
