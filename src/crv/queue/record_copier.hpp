// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <limits>
#include <span>
#include <type_traits>
#include <utility>

namespace crv {

template <typename copier_t>
concept is_byte_copier = requires(copier_t copier, std::size_t destination_offset, std::span<std::byte const> source) {
    { copier(destination_offset, source) } noexcept -> std::same_as<std::size_t>;
};

/// adapts a byte copier to the record-based spsc copier contract
///
/// This type copies whole records out of an spsc queue. The underlying copier reports bytes copied. This adapter
/// reports only complete records copied. If the byte copier stops within a record, that record is not included in the
/// returned count and so remains in the source queue.
///
/// Bytes written for an incomplete record are beyond the reported complete-record range and must be ignored by the
/// destination.
template <typename record_t, is_byte_copier byte_copier_t>
    requires(std::is_trivially_copyable_v<record_t>)
class record_copier_t
{
public:
    explicit record_copier_t(byte_copier_t copier) noexcept : copier_{std::move(copier)} {}

    auto operator()(std::size_t destination_offset, std::span<record_t const> source) noexcept -> std::size_t
    {
        assert(destination_offset <= std::numeric_limits<std::size_t>::max() / sizeof(record_t));

        auto const destination_byte_offset = destination_offset * sizeof(record_t);
        auto const source_bytes = std::as_bytes(source);
        auto const copied_bytes = copier_(destination_byte_offset, source_bytes);

        assert(copied_bytes <= source_bytes.size());

        return copied_bytes / sizeof(record_t);
    }

private:
    byte_copier_t copier_;
};

template <typename record_t, typename byte_copier_factory_t,
    typename t_product_t = record_copier_t<record_t, typename byte_copier_factory_t::product_t>>
struct record_copier_factory_t
{
    using product_t = t_product_t;

    byte_copier_factory_t byte_copier_factory;

    constexpr auto operator()() const noexcept -> product_t { return product_t{byte_copier_factory()}; }
};

} // namespace crv
