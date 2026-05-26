// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <string_view>
#include <toml++/toml.hpp>

namespace crv::serialization::tomlpp {

/// adapts toml writer to serialization::writer_t
class writer_adapter_t
{
public:
    explicit writer_adapter_t(toml::table& table) : table_{table} {}

    /// writes value under key
    template <typename value_t> auto write(std::string_view key, value_t const& value) -> void
    {
        table_.insert_or_assign(key, value);
    }

    /// creates new nested section
    auto create_section(std::string_view key) -> writer_adapter_t
    {
        auto [iterator, inserted] = table_.insert_or_assign(key, toml::table{});
        return writer_adapter_t{*iterator->second.as_table()};
    }

private:
    toml::table& table_;
};

} // namespace crv::serialization::tomlpp
