// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "config.hpp"
#include <crv/test/test.hpp>

#include <crv/ui/serialization/exceptions.hpp>
#include <filesystem>
#include <string_view>
#include <toml++/toml.hpp>

namespace crv::serialization {

namespace tomlpp {

class writer_t
{
public:
    writer_t() = default;

    template <typename value_t, typename constraint_t>
    auto visit(reflection::param_t<value_t, constraint_t> const& param) -> void
    {
        write(param.name(), param.value());
    }

    template <typename section_visitor_t>
    auto visit_section(std::string_view name, section_visitor_t&& section_visitor) -> void
    {
        auto section = writer_t{};
        std::invoke(std::forward<section_visitor_t>(section_visitor), section);
        table_.insert_or_assign(name, std::move(section.table_));
    }

    auto write(std::ostream& out) const -> void { out << table_; }

private:
    toml::table table_;

    template <typename value_t> auto write(std::string_view key, value_t const& value) -> void
    {
        table_.insert_or_assign(key, value);
    }

    template <is_enum enum_t> auto write(std::string_view key, enum_t const& value) -> void
    {
        if (auto name = reflection::to_string(value)) table_.insert_or_assign(key, *name);
        else
        {
            throw parse_x{std::format(
                "invalid value ({}) for enum \"{}\"", static_cast<std::underlying_type_t<enum_t>>(value), key)};
        }
    }
};

class reader_t
{
public:
    explicit reader_t(toml::table const& table) noexcept : table_{table} {}

    template <typename value_t, typename constraint_t>
    auto visit(reflection::param_t<value_t, constraint_t>& param) -> void
    {
        value_t value;
        if (read(param.name(), value)) param.value(std::move(value));
    }

    template <typename section_visitor_t>
    auto visit_section(std::string_view name, section_visitor_t&& section_visitor) -> void
    {
        if (auto* node = table_.get(name))
        {
            if (auto* sub_table = node->as_table())
            {
                std::invoke(std::forward<section_visitor_t>(section_visitor), reader_t{*sub_table});
            }
            else report_error(node->source(), std::format("expected table for key \"{}\"", name));
        }
    }

private:
    toml::table const& table_;

    template <typename value_t> auto read(std::string_view key, value_t& dst) -> bool
    {
        auto* node = table_.get(key);
        if (!node) return false;

        auto value = node->value<value_t>();
        if (!value)
        {
            report_error(node->source(), std::format("type mismatch for key \"{}\"", key));
            return false;
        }

        dst = std::move(*value);
        return true;
    }

    template <is_enum enum_t> auto read(std::string_view key, enum_t& dst) -> bool
    {
        auto* node = table_.get(key);
        if (!node) return false;

        auto name = node->value<std::string_view>();
        if (!name)
        {
            report_error(node->source(), std::format("type mismatch for enum key \"{}\"", key));
            return false;
        }

        if (auto value = reflection::from_string<enum_t>(*name))
        {
            dst = *value;
            return true;
        }

        report_error(node->source(), std::format("invalid value \"{}\" for enum \"{}\"", *name, key));
        return false;
    }

    [[noreturn]] auto report_error(toml::source_region location, std::string_view message) const -> void
    {
        if (location.path)
        {
            throw parse_x{
                std::format("{}:{}:{}: {}", *location.path, location.begin.line, location.begin.column, message)};
        }
        else throw parse_x{std::format("{}:{}: {}", location.begin.line, location.begin.column, message)};
    }
};

struct archive_t
{
    static auto parse(std::filesystem::path const& path) -> toml::table { return toml::parse(path.c_str()); }
    static auto parse(std::istream& in) -> toml::table { return toml::parse(in); }
    static auto create_reader(toml::table const& table) -> reader_t { return reader_t{table}; }
    static auto create_writer() -> writer_t { return {}; }
};

} // namespace tomlpp

template <typename archive_t> class serializer_t
{
public:
    template <typename reflected_t> auto operator()(reflected_t const& obj, std::ostream& out) const -> void
    {
        auto writer = archive_t::writer();
        obj.reflect(writer);
        writer.write(out);
    }
};

template <typename archive_t> class deserializer_t
{
public:
    template <typename reflected_t> auto operator()(std::istream& in, reflected_t& obj) const -> void
    {
        auto store = archive_t::parse(in);
        obj.reflect(archive_t::reader(store));
    }
};

namespace {

//

} // namespace
} // namespace crv::serialization
