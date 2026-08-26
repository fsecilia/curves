// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/kernel/abi_validation.h>
#include <crv/kernel/input/abi.h>
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstring>

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

/// bytewise adapter over c array of Linux input_value records
class input_value_array_adapter_t
{
public:
    input_value_array_adapter_t(void* values, std::size_t capacity) noexcept
        : values_{static_cast<std::byte*>(values)}, capacity_{capacity}
    {
        assert(nullptr != values_);
    }

    auto capacity() const noexcept -> std::size_t { return capacity_; }

    [[nodiscard]] auto load(std::size_t index) const noexcept -> input_value_t
    {
        assert(index < capacity_);

        crv_input_value_t result;
        std::copy_n(static_cast<std::byte const*>(address(index)), sizeof(result),
                    reinterpret_cast<std::byte*>(&result));

        return input_value_t{
            .type = static_cast<input_value_t::type_t>(result.type),
            .code = result.code,
            .value = result.value,
        };
    }

    auto store(std::size_t index, input_value_t const& value) noexcept -> void
    {
        assert(index < capacity_);

        auto const result = crv_input_value_t{
            .type = static_cast<crv_u16_t>(value.type),
            .code = value.code,
            .value = value.value,
        };

        std::copy_n(reinterpret_cast<std::byte const*>(&result), sizeof(result),
                    static_cast<std::byte*>(address(index)));
    }

    auto move(std::size_t destination, std::size_t source, std::size_t count) noexcept -> void
    {
        assert(source <= capacity_);
        assert(count <= capacity_ - source);
        assert(destination <= capacity_);
        assert(count <= capacity_ - destination);

        if (count == 0) return;
        std::memmove(address(destination), address(source), count * stride);
    }

private:
    static constexpr auto stride = sizeof(crv_input_value_t);
    auto address(std::size_t index) const noexcept -> void* { return values_ + index * stride; }

    std::byte* values_;
    std::size_t capacity_;
};

} // namespace crv
