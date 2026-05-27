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
    explicit writer_t(toml::table& table) noexcept : table_{table} {}

    template <typename value_t, typename constraint_t>
    auto visit(reflection::param_t<value_t, constraint_t> const& param) const -> void
    {
        write(param.name(), param.value());
    }

    template <typename section_visitor_t>
    auto visit_section(std::string_view name, section_visitor_t&& section_visitor) const -> void
    {
        toml::table* target_table = nullptr;

        if (auto* node = table_.get(name))
        {
            target_table = node->as_table();
            if (!target_table) throw parse_x{std::format("expected table for key \"{}\"", name)};
        }
        else
        {
            auto [iterator, inserted] = table_.insert(name, toml::table{});
            target_table = iterator->second.as_table();
        }

        std::invoke(std::forward<section_visitor_t>(section_visitor), writer_t{*target_table});
    }

private:
    toml::table& table_;

    template <typename value_t> auto write(std::string_view key, value_t const& value) const -> void
    {
        table_.insert_or_assign(key, value);
    }

    template <is_enum enum_t> auto write(std::string_view key, enum_t const& value) const -> void
    {
        auto name = reflection::to_string(value);
        if (!name)
        {
            throw parse_x{std::format(
                "invalid value ({}) for enum \"{}\"", static_cast<std::underlying_type_t<enum_t>>(value), key)};
        }

        table_.insert_or_assign(key, *name);
    }
};

class reader_t
{
public:
    explicit reader_t(toml::table const& table) noexcept : table_{table} {}

    template <typename value_t, typename constraint_t>
    auto visit(reflection::param_t<value_t, constraint_t>& param) const -> void
    {
        value_t value;
        if (read(param.name(), value)) param.value(std::move(value));
    }

    template <typename section_visitor_t>
    auto visit_section(std::string_view name, section_visitor_t&& section_visitor) const -> void
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

    template <typename value_t> auto read(std::string_view key, value_t& dst) const -> bool
    {
        auto* node = table_.get(key);
        if (!node) return false;

        auto value = node->value<value_t>();
        if (!value) report_error(node->source(), std::format("type mismatch for key \"{}\"", key));

        dst = std::move(*value);
        return true;
    }

    template <is_enum enum_t> auto read(std::string_view key, enum_t& dst) -> bool
    {
        auto* node = table_.get(key);
        if (!node) return false;

        auto name = node->value<std::string_view>();
        if (!name) report_error(node->source(), std::format("type mismatch for enum key \"{}\"", key));

        auto value = reflection::from_string<enum_t>(*name);
        if (!value) report_error(node->source(), std::format("invalid value \"{}\" for enum \"{}\"", *name, key));

        dst = *value;
        return true;
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
    using document_t = toml::table;

    auto create_read_document(std::filesystem::path const& path) const -> document_t
    {
        return toml::parse(path.c_str());
    }
    auto create_read_document(std::istream& in) const -> document_t { return toml::parse(in); }
    auto create_write_document() const -> document_t { return {}; }

    auto create_reader(document_t const& doc) const -> reader_t { return reader_t{doc}; }
    auto create_writer(document_t& doc) const -> writer_t { return writer_t{doc}; }

    auto write(document_t const& doc, std::ostream& out) const -> void { out << doc; }
};

} // namespace tomlpp

template <typename archive_t> class serializer_t
{
public:
    explicit serializer_t(archive_t archive = {}) noexcept : archive_{std::move(archive)} {}

    template <typename reflected_t>
    auto operator()(reflected_t const& obj, std::filesystem::path const& path) const -> void
    {
        deserialize(obj, std::ofstream{path});
    }

    template <typename reflected_t> auto operator()(reflected_t const& obj, std::ostream& out) const -> void
    {
        deserialize(obj, out);
    }

private:
    template <typename reflected_t> auto deserialize(reflected_t const& obj, std::ostream& out) const -> void
    {
        auto document = archive_.create_write_document();
        auto writer = archive_.create_writer(document);
        obj.reflect(writer);
        archive_.write(document, out);
    }

    archive_t archive_;
};

template <typename archive_t> class deserializer_t
{
public:
    explicit deserializer_t(archive_t archive = {}) noexcept : archive_{std::move(archive)} {}

    template <typename reflected_t> auto operator()(std::filesystem::path const& path, reflected_t& obj) const -> void
    {
        deserialize(path, obj);
    }

    template <typename reflected_t> auto operator()(std::istream& in, reflected_t& obj) const -> void
    {
        deserialize(in, obj);
    }

private:
    template <typename source_t, typename reflected_t> auto deserialize(source_t&& in, reflected_t& obj) const -> void
    {
        auto document = archive_.create_read_document(std::forward<source_t>(in));
        auto reader = archive_.create_reader(document);
        obj.reflect(reader);
    }

    archive_t archive_;
};

namespace {

//

} // namespace
} // namespace crv::serialization
