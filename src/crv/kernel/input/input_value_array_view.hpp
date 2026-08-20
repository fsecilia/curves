// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/algorithm.hpp>
#include <crv/kernel/abi_validation.h>
#include <crv/kernel/input/abi.h>
#include <cassert>
#include <cstddef>

namespace crv {

struct input_value_t
{
    enum class type_t : uint16_t
    {
        syn = CRV_EV_SYN,
        rel = CRV_EV_REL,
    };

    enum class code_syn_t : uint16_t
    {
        report = CRV_SYN_REPORT,
    };

    enum class code_rel_t : uint16_t
    {
        x = CRV_REL_X,
        y = CRV_REL_Y,
    };

    using code_t = uint16_t;
    using value_t = int32_t;

    type_t type;
    code_t code;
    value_t value;

    constexpr auto operator==(input_value_t const&) const noexcept -> bool = default;
};

/// mutable bytewise view over c array of Linux input_value records
///
/// \invariant array always ends with a {type_t::syn, code_syn_t::report} record
class input_value_array_view_t
{
public:
    input_value_array_view_t(void* values, std::size_t count, std::size_t capacity) noexcept
        : values_{static_cast<std::byte*>(values)}, count_{count}, capacity_{capacity}
    {
        assert(nullptr != values_);
        assert(0 < capacity_);
        assert(0 < count_);
        assert(count_ <= capacity_);
        assert(load(count_ - 1).type == input_value_t::type_t::syn);
        assert(load(count_ - 1).code == static_cast<input_value_t::code_t>(input_value_t::code_syn_t::report));
    }

    /// \returns number of records occupied
    auto count() const noexcept -> std::size_t { return count_; }

    /// \returns loads occupied record
    [[nodiscard]] auto load(std::size_t index) const noexcept -> input_value_t
    {
        assert(index < count_);

        crv_input_value_t result;
        __builtin_memcpy(&result, address(index), sizeof(result));

        return input_value_t{
            .type = static_cast<input_value_t::type_t>(result.type),
            .code = result.code,
            .value = result.value,
        };
    }

    /// replaces occupied record
    auto store(std::size_t index, input_value_t const& value) noexcept -> void
    {
        assert(index < count_ - 1);

        auto const result = crv_input_value_t{
            .type = static_cast<crv_u16_t>(value.type),
            .code = value.code,
            .value = value.value,
        };

        __builtin_memcpy(address(index), &result, sizeof(result));
    }

    /// appends x or y record to end of array, moving SYN_REPORT to accommodate
    auto append(input_value_t::code_rel_t code, input_value_t::value_t value) noexcept -> void
    {
        assert(count_ < capacity_);
        ++count_;

        // move SYN_REPORT right one slot
        __builtin_memmove(address(count_ - 1), address(count_ - 2), stride);

        // store new value in previous SYN_REPORT slot
        store(count_ - 2,
            input_value_t{
                .type = input_value_t::type_t::rel,
                .code = static_cast<input_value_t::code_t>(code),
                .value = value,
            });
    }

    /// appends x and y record, moving SYN_REPORT to accommodate
    auto append(input_value_t::value_t x, input_value_t::value_t y) noexcept -> void
    {
        assert(count_ < capacity_);
        assert(capacity_ - count_ >= 2);
        count_ += 2;

        // move SYN_REPORT right two slots
        __builtin_memmove(address(count_ - 1), address(count_ - 3), stride);

        // store new (x, y) value starting in previous SYN_REPORT slot
        store(count_ - 3,
            input_value_t{
                .type = input_value_t::type_t::rel,
                .code = static_cast<input_value_t::code_t>(input_value_t::code_rel_t::x),
                .value = x,
            });
        store(count_ - 2,
            input_value_t{
                .type = input_value_t::type_t::rel,
                .code = static_cast<input_value_t::code_t>(input_value_t::code_rel_t::y),
                .value = y,
            });
    }

    /// erases an x or y record, moving SYN_REPORT to accommodate
    auto erase(std::size_t index) noexcept -> void
    {
        assert(count_ > 1);
        assert(index < count_ - 1);
        assert(load(index).type == input_value_t::type_t::rel);
        assert(load(index).code == static_cast<input_value_t::code_t>(input_value_t::code_rel_t::x)
            || load(index).code == static_cast<input_value_t::code_t>(input_value_t::code_rel_t::y));

        __builtin_memmove(address(index), address(index + 1), (count_ - index - 1) * stride);
        --count_;
    }

    /// erases x and y record, moving SYN_REPORT to accommodate
    auto erase(std::size_t x_index, std::size_t y_index) noexcept -> void
    {
        assert(count_ > 2);
        assert(x_index < count_ - 1);
        assert(y_index < count_ - 1);
        assert(x_index != y_index);
        assert(load(x_index).type == input_value_t::type_t::rel);
        assert(load(x_index).code == static_cast<input_value_t::code_t>(input_value_t::code_rel_t::x));
        assert(load(y_index).type == input_value_t::type_t::rel);
        assert(load(y_index).code == static_cast<input_value_t::code_t>(input_value_t::code_rel_t::y));

        auto const lower = min(x_index, y_index);
        auto const upper = max(x_index, y_index);

        // shift records between erased elements left by one
        if (upper > lower + 1) __builtin_memmove(address(lower), address(lower + 1), (upper - lower - 1) * stride);

        // shift remaining records left by two
        __builtin_memmove(address(upper - 1), address(upper + 1), (count_ - upper - 1) * stride);

        count_ -= 2;
    }

private:
    static constexpr auto stride = sizeof(crv_input_value_t);
    auto address(std::size_t index) const noexcept -> void* { return values_ + index * stride; }

    std::byte* values_;
    std::size_t count_;
    [[maybe_unused]] std::size_t capacity_;
};

} // namespace crv
