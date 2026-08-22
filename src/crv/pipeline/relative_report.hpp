// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/pipeline/input_frame.hpp>
#include <cstddef>

namespace crv::pipeline {

/// relative report with all-or-nothing output updates
class relative_report_t
{
public:
    using value_t = input_value_t::value_t;

    explicit relative_report_t(input_frame_t& frame) noexcept : frame_{frame}
    {
        if (!frame_.valid()) return;

        for (auto index = std::size_t{0}; index + 1 < frame_.count(); ++index)
        {
            auto const value = frame_.load(index);
            if (value.type != input_value_t::type_t::rel) continue;

            if (value.code == static_cast<input_value_t::code_t>(input_value_t::code_rel_t::x))
            {
                if (has_x_) return;

                x_ = value.value;
                x_index_ = index;
                has_x_ = true;
            }
            else if (value.code == static_cast<input_value_t::code_t>(input_value_t::code_rel_t::y))
            {
                if (has_y_) return;

                y_ = value.value;
                y_index_ = index;
                has_y_ = true;
            }
        }

        valid_ = true;
    }

    relative_report_t(relative_report_t const&) = delete;
    relative_report_t(relative_report_t&&) = delete;
    auto operator=(relative_report_t const&) -> relative_report_t& = delete;
    auto operator=(relative_report_t&&) -> relative_report_t& = delete;

    constexpr auto valid() const noexcept -> bool { return valid_; }
    constexpr auto x() const noexcept -> value_t { return x_; }
    constexpr auto y() const noexcept -> value_t { return y_; }

    /// stores final x/y only after any needed append succeeds
    ///
    /// Missing axes stay absent when final output is zero. Existing axes that become zero are erased. A report with
    /// one original axis may append the other; a report with no original x/y motion must remain without x/y motion.
    ///
    /// \pre final report mutation has not already been attempted
    [[nodiscard]] auto try_store(value_t x, value_t y) && noexcept -> bool
    {
        if (!valid_) return false;

        if (!has_x_ && !has_y_)
        {
            assert(x == 0 && y == 0 && "relative_report_t: output motion without original x/y motion");
            return x == 0 && y == 0;
        }

        if (!has_x_ && x != 0 && !frame_.try_append(rel(input_value_t::code_rel_t::x, x))) return false;
        if (!has_y_ && y != 0 && !frame_.try_append(rel(input_value_t::code_rel_t::y, y))) return false;

        if (has_x_ && x != 0) store(x_index_, input_value_t::code_rel_t::x, x);
        if (has_y_ && y != 0) store(y_index_, input_value_t::code_rel_t::y, y);

        if (has_x_ && x == 0 && has_y_ && y == 0) frame_.erase(x_index_, y_index_);
        else if (has_x_ && x == 0) frame_.erase(x_index_);
        else if (has_y_ && y == 0) frame_.erase(y_index_);

        return true;
    }

private:
    static constexpr auto rel(input_value_t::code_rel_t code, value_t value) noexcept -> input_value_t
    {
        return {
            .type = input_value_t::type_t::rel,
            .code = static_cast<input_value_t::code_t>(code),
            .value = value,
        };
    }

    auto store(std::size_t index, input_value_t::code_rel_t code, value_t value) noexcept -> void
    {
        frame_.store(index,
            input_value_t{
                .type = input_value_t::type_t::rel,
                .code = static_cast<input_value_t::code_t>(code),
                .value = value,
            });
    }

    // metadata describes the original inspected frame
    input_frame_t& frame_;
    value_t x_{};
    value_t y_{};
    std::size_t x_index_{};
    std::size_t y_index_{};
    bool has_x_ = false;
    bool has_y_ = false;
    bool valid_ = false;
};

} // namespace crv::pipeline
