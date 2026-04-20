// SPDX-License-Identifier: MIT

/// \file
/// \brief hardware prefetcher
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <concepts>
#include <cstddef>
#include <new>
#include <ranges>
#include <span>
#include <utility>

namespace crv {

template <typename prefetcher_t>
concept is_prefetcher = requires(prefetcher_t prefetcher, void const* begin, void const* end, int_t cache_line_count) {
    { prefetcher.prefetch(begin) } -> std::same_as<void>;
    { prefetcher.template prefetch<1>(begin) } -> std::same_as<void>;
    { prefetcher.prefetch(begin, cache_line_count) } -> std::same_as<void>;

    {
        prefetcher.prefetch(std::span{static_cast<std::byte const*>(begin), static_cast<std::byte const*>(end)})
    } -> std::same_as<void>;
};

namespace hints {
namespace generic {

/// external api seam
struct api_t
{
    /// prefetches address using parameterized access and locality
    template <int locality> constexpr auto prefetch(void const* address) const noexcept -> void
    {
        __builtin_prefetch(address, 0, locality); // 0 = read-only
    }
};

/// generic hint
template <int locality, typename api_t> struct hint_t
{
    [[no_unique_address]] api_t api{};

    /// prefetches address using parameterized access and locality
    constexpr auto prefetch(void const* address) const noexcept -> void { api.template prefetch<locality>(address); }
};

} // namespace generic

/// hint for read-only streaming data
///
/// This hint is used for nontemporal data that is streamed and consumed once.
using stream_t = generic::hint_t<0, generic::api_t>; // 0 = low temporal locality

/// hint for read-only static data
///
/// This hint is used for temporal data that is cached and consumed many times.
using static_t = generic::hint_t<3, generic::api_t>; // 3 = high temporal locality

} // namespace hints

/// hardware prefetcher wrapping __builtin_prefetch
template <typename hint_t> struct prefetcher_t
{
    [[no_unique_address]] hint_t hint;

    static constexpr auto cache_line_size = std::hardware_constructive_interference_size;

    /// prefetches cache line containing prefetch
    constexpr auto prefetch(void const* address) const noexcept -> void { hint.prefetch(address); }

    /// prefetches counted range with compile-time bounds
    template <int_t cache_line_count> constexpr auto prefetch(void const* address) const noexcept -> void
    {
        prefetch_sequence(static_cast<std::byte const*>(address), std::make_index_sequence<cache_line_count>{});
    }

    /// prefetches counted range with runtime bounds
    constexpr auto prefetch(void const* address, std::integral auto cache_line_count) const noexcept -> void
    {
        auto const* current = static_cast<std::byte const*>(address);
        for (auto i = decltype(cache_line_count){0}; i < cache_line_count; ++i) prefetch(current + i * cache_line_size);
    }

    /// prefetches range
    constexpr auto prefetch(std::ranges::contiguous_range auto&& range) const noexcept -> void
    {
        auto const* cur = static_cast<std::byte const*>(std::ranges::data(range));
        auto const* end = cur + std::ranges::size(range) * sizeof(std::ranges::range_value_t<decltype(range)>);

        while (cur < end)
        {
            prefetch(cur);
            cur += cache_line_size;
        }
    }

private:
    template <std::size_t... indices>
    constexpr auto prefetch_sequence(std::byte const* address, std::index_sequence<indices...>) const noexcept -> void
    {
        /// right fold to guarantee repetition at compile time
        (prefetch(address + indices * cache_line_size), ...);
    }
};

/// hardware prefetcher to stream ro data
using streaming_prefetcher_t = prefetcher_t<hints::stream_t>;

/// hardware prefetcher to cache ro data
using static_prefetcher_t = prefetcher_t<hints::static_t>;

} // namespace crv
