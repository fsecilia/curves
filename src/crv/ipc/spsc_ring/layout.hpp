// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <cstddef>

namespace crv::ipc::spsc_ring {

using sequence_t = uint64_t;

inline constexpr auto magic = uint32_t{0x43525652}; // "CRVR"
inline constexpr auto abi_major = uint16_t{1};
inline constexpr auto abi_minor = uint16_t{0};
inline constexpr auto alignment = std::size_t{64};

struct alignas(alignment) description_t
{
    uint32_t magic{};
    uint16_t abi_major{};
    uint16_t abi_minor{};

    uint32_t element_size{};
    uint32_t element_alignment{};

    sequence_t capacity{};
    sequence_t data_offset{};

    std::byte reserved[32]{};
};

struct alignas(alignment) producer_state_t
{
    sequence_t head{};
    sequence_t dropped{};

    std::byte reserved[48]{};
};

struct alignas(alignment) consumer_state_t
{
    sequence_t tail{};

    std::byte reserved[56]{};
};

struct alignas(alignment) header_t
{
    description_t description{};
    producer_state_t producer{};
    consumer_state_t consumer{};
};

template <typename element_t> constexpr auto data_offset() noexcept -> size_t
{
    static_assert(alignof(element_t) <= alignof(header_t));
    return sizeof(header_t);
}

template <typename element_t> constexpr auto byte_size(sequence_t capacity) noexcept -> size_t
{
    return data_offset<element_t>() + static_cast<size_t>(capacity) * sizeof(element_t);
}

template <typename element_t> auto to_elements(void const* memory) noexcept -> element_t const*
{
    return reinterpret_cast<element_t const*>(static_cast<std::byte const*>(memory) + data_offset<element_t>());
}

template <typename element_t> auto to_elements(void* memory) noexcept -> element_t*
{
    return const_cast<element_t*>(to_elements<element_t>(static_cast<void const*>(memory)));
}

} // namespace crv::ipc::spsc_ring
