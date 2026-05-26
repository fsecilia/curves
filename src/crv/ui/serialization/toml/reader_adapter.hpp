// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <format>
#include <optional>
#include <string_view>
#include <toml++/toml.hpp>

namespace crv::serialization::tomlpp {

/// adapts toml reader to serialization::reader_t
template <typename error_reporter_t> class reader_adapter_t
{
public:
    explicit reader_adapter_t(toml::table const& table, error_reporter_t& error_reporter)
        : table_{table}, error_reporter_{error_reporter}
    {}

    template <typename value_t> auto read(std::string_view key, value_t& dst) -> void
    {
        if (auto* node = table_.get(key))
        {
            error_reporter_.location(node->source());
            if (auto val = node->value<value_t>()) dst = std::move(*val);
            else error_reporter_.report_error(std::format("type mismatch for key \"{}\"", key));
        }
    }

    auto get_section(std::string_view key) -> std::optional<reader_adapter_t>
    {
        if (auto* node = table_.get(key))
        {
            error_reporter_.location(node->source());
            if (auto* sub_table = node->as_table()) return reader_adapter_t{*sub_table, error_reporter_};
            else error_reporter_.report_error(std::format("expected table/section for key \"{}\"", key));
        }
        return std::nullopt;
    }

private:
    toml::table const& table_;
    error_reporter_t& error_reporter_;
};

} // namespace crv::serialization::tomlpp
