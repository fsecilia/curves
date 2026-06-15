// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <cassert>
#include <utility>
#include <vector>

namespace crv::command {

/// navigable sequence of events with explicit history and future
template <typename t_event_t, std::size_t initial_capacity = 32> class timeline_t
{
public:
    using event_t = t_event_t;

    using events_t = std::vector<event_t>;

    constexpr timeline_t() noexcept = default;

    constexpr timeline_t(timeline_t&&) noexcept = default;
    constexpr auto operator=(timeline_t&&) noexcept -> timeline_t& = default;

    /// \returns total capacity for all events, future and past
    constexpr auto capacity() const noexcept -> events_t::size_type { return events_.capacity(); }

    /// reserves capacity for next call to commit
    constexpr auto reserve() -> void
    {
        if (can_commit()) return;
        auto const capacity = std::max(initial_capacity, events_.capacity() * 2);
        events_.reserve(capacity);
    }

    /// \returns true when commit() can be safely applied.
    [[nodiscard]] constexpr auto can_commit() const noexcept -> bool
    {
        return future_ < events_.size() || events_.size() < events_.capacity();
    }

    /// commits event to timeline as new present, truncating the future branch
    ///
    /// \pre can_commit()
    constexpr auto commit(event_t event) noexcept -> event_t&
    {
        assert(can_commit());
        events_.resize(future_);
        events_.push_back(std::move(event));
        auto& result = events_.back();
        step_forward();
        return result;
    }

    /// \returns final event in history branch
    ///
    /// \pre can_step_backward()
    [[nodiscard]] constexpr auto present(this auto&& self) noexcept -> decltype(auto)
    {
        assert(self.can_step_backward() && self.future_ - 1 < self.events_.size());
        return self.events_[self.future_ - 1];
    }

    /// \returns first event in future branch
    ///
    /// \pre can_step_forward()
    [[nodiscard]] constexpr auto future(this auto&& self) noexcept -> decltype(auto)
    {
        assert(self.can_step_forward());
        return self.events_[self.future_];
    }

    /// \returns true before reaching end of timeline
    [[nodiscard]] constexpr auto can_step_forward() const noexcept -> bool { return future_ < events_.size(); }

    /// \returns true before reaching beginning of timeline
    [[nodiscard]] constexpr auto can_step_backward() const noexcept -> bool { return 0 < future_; }

    /// moves present one event into future
    constexpr auto step_forward() noexcept -> void
    {
        assert(can_step_forward());
        ++future_;
    }

    /// moves present one event into past
    constexpr auto step_backward() noexcept -> void
    {
        assert(can_step_backward());
        --future_;
    }

    /// clears all events
    constexpr auto clear() noexcept -> void
    {
        events_.clear();
        future_ = 0;
    }

private:
    events_t events_;
    events_t::size_type future_ = 0;
};

} // namespace crv::command
