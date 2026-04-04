// SPDX-License-Identifier: MIT

/// \file
/// \brief extended priority queue
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <algorithm>
#include <functional>
#include <utility>

namespace crv {

/// priority queue with minimal container api
///
/// std::priority_queue_t notoriously does not offer a .clear() method or means to reserve capacity after
/// initialization. This type extends the api to include a minimal set of methods to interact with the underlying
/// container.
template <typename container_t, typename compare_t = std::less<>, typename projection_t = std::identity>
class priority_queue_t
{
public:
    constexpr priority_queue_t() = default;
    constexpr priority_queue_t(container_t container, compare_t compare = {}, projection_t projection = {}) noexcept
        : container_{std::move(container)}, compare_{std::move(compare)}, projection_{std::move(projection)}
    {
        std::ranges::make_heap(container_, compare_, projection_);
    }

    template <typename... args_t> constexpr auto emplace(args_t&&... args) -> void
    {
        container_.emplace_back(std::forward<args_t>(args)...);
        std::ranges::push_heap(container_, compare_, projection_);
    }

    constexpr auto push(container_t::value_type const& value) -> void { emplace(value); }
    constexpr auto push(container_t::value_type&& value) -> void { emplace(std::move(value)); }
    constexpr auto pop() noexcept -> void
    {
        std::ranges::pop_heap(container_, compare_, projection_);
        container_.pop_back();
    }

    constexpr auto top() const noexcept -> container_t::value_type const& { return container_.front(); }
    constexpr auto clear() noexcept -> void { container_.clear(); }
    constexpr auto empty() const noexcept -> bool { return container_.empty(); }
    constexpr auto size() const noexcept -> std::size_t { return container_.size(); }
    constexpr auto capacity() const noexcept -> std::size_t { return container_.capacity(); }
    constexpr auto reserve(std::size_t capacity) -> void { container_.reserve(capacity); }

private:
    container_t                        container_{};
    [[no_unique_address]] compare_t    compare_{};
    [[no_unique_address]] projection_t projection_{};
};

} // namespace crv
