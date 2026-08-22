// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/algorithm.hpp>
#include <crv/kernel/input/input_value_array_adapter.hpp>
#include <cassert>
#include <cstddef>

namespace crv::pipeline {

/// mutable logical Linux input frame
class input_frame_t
{
public:
    input_frame_t(input_value_array_adapter_t& values, std::size_t count) noexcept : values_{values}, count_{count}
    {
        if (count_ == 0 || count_ > values_.capacity()) return;

        auto const terminator = values_.load(count_ - 1);
        if (terminator.type != input_value_t::type_t::syn) return;
        if (terminator.code != static_cast<input_value_t::code_t>(input_value_t::code_syn_t::report)) return;

        valid_ = true;
    }

    input_frame_t(input_frame_t const&) = delete;
    input_frame_t(input_frame_t&&) = delete;
    auto operator=(input_frame_t const&) -> input_frame_t& = delete;
    auto operator=(input_frame_t&&) -> input_frame_t& = delete;

    auto valid() const noexcept -> bool { return valid_; }
    auto count() const noexcept -> std::size_t { return count_; }

    [[nodiscard]] auto load(std::size_t index) const noexcept -> input_value_t
    {
        assert(valid_);
        assert(index < count_);
        return values_.load(index);
    }

    auto store(std::size_t index, input_value_t const& value) noexcept -> void
    {
        assert(valid_);
        assert(index < count_ - 1);
        values_.store(index, value);
    }

    /// appends payload record before SYN_REPORT
    [[nodiscard]] auto try_append(input_value_t const& value) noexcept -> bool
    {
        assert(valid_);
        if (count_ == values_.capacity()) return false;

        values_.move(count_, count_ - 1, 1);
        values_.store(count_ - 1, value);
        ++count_;
        return true;
    }

    /// erases payload record
    auto erase(std::size_t index) noexcept -> void
    {
        assert(valid_);
        assert(count_ > 1);
        assert(index < count_ - 1);

        values_.move(index, index + 1, count_ - index - 1);
        --count_;
    }

    /// erases two payload records
    auto erase(std::size_t first_index, std::size_t second_index) noexcept -> void
    {
        assert(valid_);
        assert(count_ > 2);
        assert(first_index < count_ - 1);
        assert(second_index < count_ - 1);
        assert(first_index != second_index);

        auto const lower = min(first_index, second_index);
        auto const upper = max(first_index, second_index);

        // shift records between erased elements left by one
        if (upper > lower + 1) values_.move(lower, lower + 1, upper - lower - 1);

        // shift remaining records left by two
        values_.move(upper - 1, upper + 1, count_ - upper - 1);
        count_ -= 2;
    }

private:
    input_value_array_adapter_t& values_;
    std::size_t count_;
    bool valid_ = false;
};

} // namespace crv::pipeline
