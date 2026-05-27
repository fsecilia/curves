// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/ui/reflection/enum.hpp>
#include <crv/ui/serialization/exceptions.hpp>
#include <string_view>
#include <toml++/toml.hpp>

namespace crv::serialization::tomlpp {

/// adapts toml writer to serialization::writer_t
class writer_adapter_t
{
public:
    explicit writer_adapter_t(toml::table& table) : table_{table} {}

    /// writes generic value under key
    template <typename value_t> auto write(std::string_view key, value_t const& value) -> void
    {
        table_.insert_or_assign(key, value);
    }

    /// writes enum value under param's key; reports error if enum is invalid
    ///
    /// \throws if value is not a valid member of enum
    template <is_enum enum_t> auto write(std::string_view key, enum_t const& value) -> void
    {
        if (auto const opt_enum = reflection::to_string(value)) write(key, *opt_enum);
        else throw parse_x{std::format("invalid value ({}) for enum \"{}\"", static_cast<int_t>(value), key)};
    }

    /// appends to existing section, or creates a new one
    [[nodiscard]] auto find_or_create_section(std::string_view key) -> writer_adapter_t
    {
        // try and find existing table
        if (auto* node = table_.get(key))
        {
            // existing nested table was found; wrap it in a nested adapter
            auto target_table = node->as_table();
            if (!target_table) throw parse_x{std::format("expected table for key \"{}\"", key)};
            return writer_adapter_t{*target_table};
        }
        else
        {
            // create new nested table and wrap it in a nested adapter
            auto [iterator, inserted] = table_.insert(key, toml::table{});
            return writer_adapter_t{*iterator->second.as_table()};
        }
    }

private:
    toml::table& table_;
};

} // namespace crv::serialization::tomlpp
