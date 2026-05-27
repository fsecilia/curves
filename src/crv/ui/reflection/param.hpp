// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/ui/reflection/constraints.hpp>
#include <string_view>
#include <utility>

namespace crv::reflection {

/// named reflection wrapper
///
/// This type wraps a value to imbue it with named reflection and an optional constraint.
template <typename value_t, typename constraint_t = constraints::none_t> class param_t
{
public:
    constexpr param_t(std::string_view name, value_t value)
        requires std::is_default_constructible_v<constraint_t>
        : param_t{name, std::move(value), constraint_t{}}
    {}

    constexpr param_t(std::string_view name, value_t value, constraint_t constraint)
        : name_{name}, value_{constraint(value)}, constraint_{std::move(constraint)}
    {}

    constexpr auto name() const noexcept -> std::string_view { return name_; }
    constexpr auto value() const noexcept -> value_t { return value_; }

    constexpr auto value(value_t value) noexcept -> bool
    {
        auto constrained_value = constraint_(std::move(value));
        value_ = std::move(constrained_value);
        return constrained_value != value;
    }

    constexpr auto constraint() const noexcept -> constraint_t const& { return constraint_; }

    constexpr auto reflect(this auto&& self, auto&& visitor) -> decltype(auto)
    {
        visitor.visit(std::forward<decltype(self)>(self));
        return std::forward<decltype(visitor)>(visitor);
    }

    constexpr auto operator==(param_t const&) const noexcept -> bool = default;

private:
    std::string_view name_;
    value_t value_;
    constraint_t constraint_;
};

} // namespace crv::reflection
