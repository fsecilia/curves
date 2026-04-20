// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "prefetcher.hpp"
#include <crv/test/test.hpp>
#include <array>
#include <cassert>

namespace crv {

// --------------------------------------------------------------------------------------------------------------------
// concept
// --------------------------------------------------------------------------------------------------------------------

static_assert(is_prefetcher<streaming_prefetcher_t>);
static_assert(is_prefetcher<static_prefetcher_t>);

namespace hints::generic {
namespace {

// --------------------------------------------------------------------------------------------------------------------
// hint_t
// --------------------------------------------------------------------------------------------------------------------

struct tracking_api_t
{
    void const* last_address  = nullptr;
    int         last_locality = -1;

    template <int locality> constexpr auto prefetch(void const* address) noexcept -> void
    {
        last_address  = address;
        last_locality = locality;
    }
};

template <int locality> constexpr auto test_hint() noexcept -> bool
{
    using sut_t = hint_t<locality, tracking_api_t&>;

    auto       api = tracking_api_t{};
    auto const sut = sut_t{api};

    auto const target = int_t{};
    sut.prefetch(&target);

    return api.last_address == &target && api.last_locality == locality;
}
static_assert(test_hint<0>());
static_assert(test_hint<3>());

} // namespace
} // namespace hints::generic

namespace {

struct tracking_hint_t
{
    constexpr auto prefetch(void const* address) -> void
    {
        assert(prefetch_count < std::size(prefetched_addresses));
        prefetched_addresses[prefetch_count++] = address;
    }

    std::array<void const*, 8> prefetched_addresses{};
    std::size_t                prefetch_count = 0;
};

using sut_t                    = prefetcher_t<tracking_hint_t&>;
constexpr auto cache_line_size = sut_t::cache_line_size;

// --------------------------------------------------------------------------------------------------------------------
// single
// --------------------------------------------------------------------------------------------------------------------

constexpr auto test_single() noexcept -> bool
{
    auto       hint = tracking_hint_t{};
    auto const sut  = sut_t{hint};

    auto const target = int_t{};
    sut.prefetch(&target);

    return hint.prefetch_count == 1 && hint.prefetched_addresses[0] == &target;
};
static_assert(test_single());

// --------------------------------------------------------------------------------------------------------------------
// compile-time counted range
// --------------------------------------------------------------------------------------------------------------------

constexpr auto test_compile_time_counted_range() noexcept -> bool
{
    auto       hint = tracking_hint_t{};
    auto const sut  = sut_t{hint};

    constexpr auto cache_line_count = 3;
    auto           target           = std::array<std::byte, cache_line_count * cache_line_size>{};

    sut.template prefetch<cache_line_count>(std::data(target));

    return hint.prefetch_count == cache_line_count && hint.prefetched_addresses[0] == &target[0]
           && hint.prefetched_addresses[1] == &target[cache_line_size]
           && hint.prefetched_addresses[cache_line_count - 1] == &target[cache_line_size * (cache_line_count - 1)];
};
static_assert(test_compile_time_counted_range());

constexpr auto test_compile_time_counted_range_misaligned() noexcept -> bool
{
    auto       hint = tracking_hint_t{};
    auto const sut  = sut_t{hint};

    constexpr auto cache_line_count = 3;
    constexpr auto misalignment     = 5;
    auto           target           = std::array<std::byte, cache_line_count * cache_line_size + misalignment>{};
    sut.template prefetch<cache_line_count>(std::data(target) + misalignment);

    return hint.prefetch_count == cache_line_count && hint.prefetched_addresses[0] == &target[0] + misalignment
           && hint.prefetched_addresses[1] == &target[cache_line_size] + misalignment
           && hint.prefetched_addresses[cache_line_count - 1]
                  == &target[cache_line_size * (cache_line_count - 1)] + misalignment;
};
static_assert(test_compile_time_counted_range_misaligned());

constexpr auto test_compile_time_counted_range_empty() noexcept -> bool
{
    auto       hint = tracking_hint_t{};
    auto const sut  = sut_t{hint};

    constexpr auto cache_line_count = 0;
    auto           target           = std::array<std::byte, cache_line_count * cache_line_size>{};

    sut.template prefetch<cache_line_count>(std::data(target));

    return hint.prefetch_count == cache_line_count;
};
static_assert(test_compile_time_counted_range_empty());

// --------------------------------------------------------------------------------------------------------------------
// runtime counted range
// --------------------------------------------------------------------------------------------------------------------

constexpr auto test_runtime_counted_range() noexcept -> bool
{
    auto       hint = tracking_hint_t{};
    auto const sut  = sut_t{hint};

    constexpr auto cache_line_count = 5;
    auto           target           = std::array<std::byte, cache_line_count * cache_line_size>{};

    sut.prefetch(std::data(target), cache_line_count);

    return hint.prefetch_count == cache_line_count && hint.prefetched_addresses[0] == &target[0]
           && hint.prefetched_addresses[1] == &target[cache_line_size]
           && hint.prefetched_addresses[2] == &target[cache_line_size * 2]
           && hint.prefetched_addresses[3] == &target[cache_line_size * 3]
           && hint.prefetched_addresses[cache_line_count - 1] == &target[cache_line_size * (cache_line_count - 1)];
};
static_assert(test_runtime_counted_range());

constexpr auto test_runtime_counted_range_misaligned() noexcept -> bool
{
    auto       hint = tracking_hint_t{};
    auto const sut  = sut_t{hint};

    constexpr auto cache_line_count = 5;
    constexpr auto misalignment     = 3;
    auto           target           = std::array<std::byte, cache_line_count * cache_line_size + misalignment>{};

    sut.prefetch(std::data(target) + misalignment, cache_line_count);

    return hint.prefetch_count == cache_line_count && hint.prefetched_addresses[0] == &target[0] + misalignment
           && hint.prefetched_addresses[1] == &target[cache_line_size] + misalignment
           && hint.prefetched_addresses[2] == &target[cache_line_size * 2] + misalignment
           && hint.prefetched_addresses[3] == &target[cache_line_size * 3] + misalignment
           && hint.prefetched_addresses[cache_line_count - 1]
                  == &target[cache_line_size * (cache_line_count - 1)] + misalignment;
};
static_assert(test_runtime_counted_range_misaligned());

constexpr auto test_runtime_counted_range_empty() noexcept -> bool
{
    auto       hint = tracking_hint_t{};
    auto const sut  = sut_t{hint};

    constexpr auto cache_line_count = 0;
    auto           target           = std::array<std::byte, cache_line_count * cache_line_size>{};

    sut.prefetch(std::data(target), cache_line_count);

    return hint.prefetch_count == cache_line_count;
};
static_assert(test_runtime_counted_range_empty());

// --------------------------------------------------------------------------------------------------------------------
// range
// --------------------------------------------------------------------------------------------------------------------

constexpr auto test_range() noexcept -> bool
{
    auto       hint = tracking_hint_t{};
    auto const sut  = sut_t{hint};

    constexpr auto cache_line_count = 7;
    auto           target           = std::array<std::byte, cache_line_count * cache_line_size>();

    sut.prefetch(std::span{std::data(target), std::data(target) + std::size(target)});

    return hint.prefetch_count == cache_line_count && hint.prefetched_addresses[0] == &target[0]
           && hint.prefetched_addresses[1] == &target[cache_line_size]
           && hint.prefetched_addresses[2] == &target[cache_line_size * 2]
           && hint.prefetched_addresses[3] == &target[cache_line_size * 3]
           && hint.prefetched_addresses[4] == &target[cache_line_size * 4]
           && hint.prefetched_addresses[5] == &target[cache_line_size * 5]
           && hint.prefetched_addresses[cache_line_count - 1] == &target[cache_line_size * (cache_line_count - 1)];
};
static_assert(test_range());

constexpr auto test_range_misaligned() noexcept -> bool
{
    auto       hint = tracking_hint_t{};
    auto const sut  = sut_t{hint};

    constexpr auto cache_line_count = 7;
    constexpr auto misalignment     = 3;
    auto           target           = std::array<std::byte, cache_line_count * cache_line_size + misalignment>{};

    sut.prefetch(std::span{std::data(target) + misalignment, std::data(target) + std::size(target)});

    return hint.prefetch_count == cache_line_count && hint.prefetched_addresses[0] == &target[0] + misalignment
           && hint.prefetched_addresses[1] == &target[cache_line_size] + misalignment
           && hint.prefetched_addresses[2] == &target[cache_line_size * 2] + misalignment
           && hint.prefetched_addresses[3] == &target[cache_line_size * 3] + misalignment
           && hint.prefetched_addresses[4] == &target[cache_line_size * 4] + misalignment
           && hint.prefetched_addresses[5] == &target[cache_line_size * 5] + misalignment
           && hint.prefetched_addresses[cache_line_count - 1]
                  == &target[cache_line_size * (cache_line_count - 1)] + misalignment;
};
static_assert(test_range_misaligned());

constexpr auto test_range_empty() noexcept -> bool
{
    auto       hint = tracking_hint_t{};
    auto const sut  = sut_t{hint};

    constexpr auto cache_line_count = 0;
    auto           target           = std::array<std::byte, cache_line_count * cache_line_size>();

    sut.prefetch(std::span{std::data(target), std::data(target) + std::size(target)});

    return hint.prefetch_count == cache_line_count;
};
static_assert(test_range_empty());

} // namespace
} // namespace crv
