// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/concepts.hpp>
#include <crv/ui/reflection/enum.hpp>
#include <crv/ui/serialization/exceptions.hpp>
#include <format>
#include <optional>
#include <string_view>
#include <toml++/toml.hpp>

namespace crv::serialization::tomlpp {

/// adapts toml reader to serialization::reader_t
class reader_adapter_t
{
public:
    explicit reader_adapter_t(toml::table const& table) noexcept : table_{table} {}

    /// reads dst under key if present; reports error if types do not match
    ///
    /// \throws if key is present, but types do not match
    /// \returns true if key is present and matches type
    template <typename value_t> auto read(std::string_view key, value_t& dst) -> bool
    {
        auto* node = table_.get(key);
        if (!node) return false;

        auto value = node->value<value_t>();
        if (!value) report_error(node->source(), std::format("type mismatch for key \"{}\"", key));

        dst = std::move(*value);
        return true;
    }

    /// reads dst under key if present; reports error if types do not match
    ///
    /// \throws if key is present, but is not an enum
    /// \throws if key is present, but not a member of enum
    /// \returns true if key is present and matches enum type and value is valid enum member
    template <is_enum enum_t> auto read(std::string_view key, enum_t& dst) -> bool
    {
        auto* node = table_.get(key);
        if (!node) return false;

        auto const name = node->value<std::string_view>();
        if (!name) report_error(node->source(), std::format("type mismatch for enum key \"{}\"", key));

        auto const value = reflection::from_string<enum_t>(*name);
        if (!value) report_error(node->source(), std::format("invalid value \"{}\" for enum \"{}\"", *name, key));

        dst = *value;
        return true;
    }

    /// nests into section if present; reports error if type is not a section
    ///
    /// \throws if section key is present, but does not contain a section
    /// \returns nested reader if section is present, nullopt otherwise
    [[nodiscard]] auto get_section(std::string_view key) -> std::optional<reader_adapter_t>
    {
        if (auto* node = table_.get(key))
        {
            if (auto* sub_table = node->as_table()) return reader_adapter_t{*sub_table};
            else report_error(node->source(), std::format("expected table/section for key \"{}\"", key));
        }
        return std::nullopt;
    }

private:
    toml::table const& table_;

    [[noreturn]] auto report_error(toml::source_region location_, std::string_view message) const -> void
    {
        if (location_.path)
        {
            throw parse_x{
                std::format("{}:{}:{}: {}", *location_.path, location_.begin.line, location_.begin.column, message)};
        }
        else
        {
            throw parse_x{std::format("{}:{}: {}", location_.begin.line, location_.begin.column, message)};
        }
    }
};

} // namespace crv::serialization::tomlpp
