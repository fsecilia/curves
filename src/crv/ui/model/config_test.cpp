// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "config.hpp"
#include <crv/test/test.hpp>

#include <crv/ui/serialization/exceptions.hpp>
#include <crv/ui/serialization/toml/error_reporting.hpp>
#include <crv/ui/serialization/toml/reader_adapter.hpp>
#include <crv/ui/serialization/toml/writer_adapter.hpp>
#include <filesystem>
#include <string_view>
#include <toml++/toml.hpp>

namespace crv::serialization {

namespace tomlpp {

struct archive_t
{
    static constexpr auto file_extension = std::string_view{".toml"};

    using document_t = toml::table;

    auto create_read_document(std::filesystem::path const& path) const -> document_t
    {
        try
        {
            return toml::parse(path.c_str());
        }
        catch (toml::parse_error const& exception)
        {
            report_error(exception.what(), exception.source());
        }
    }

    auto create_read_document(std::istream& in) const -> document_t
    {
        try
        {
            return toml::parse(in);
        }
        catch (toml::parse_error const& exception)
        {
            report_error(exception.what());
        }
    }

    auto create_write_document() const -> document_t { return {}; }

    auto create_reader_adapter(document_t const& document) const -> reader_adapter_t
    {
        return reader_adapter_t{document};
    }

    auto create_writer_adapter(document_t& document) const -> writer_adapter_t { return writer_adapter_t{document}; }

    auto write(document_t const& document, std::ostream& out) const -> void
    {
        out << document;
        if (!out) throw io_x{"failed to write TOML document to output stream"};
    }

    auto write(document_t const& document, std::filesystem::path const& path) const -> void
    {
        auto out = std::ofstream{path};
        if (!out.is_open()) throw io_x{"failed to open file for writing: " + path.string()};

        out << document;
        if (!out) throw io_x{"failed to write TOML document to file: " + path.string()};
    }
};

} // namespace tomlpp

template <typename adapter_t> class writer_t
{
public:
    explicit writer_t(adapter_t adapter) noexcept : adapter_{std::move(adapter)} {}

    template <typename value_t, typename constraint_t>
    auto visit(reflection::param_t<value_t, constraint_t> const& param) const -> void
    {
        adapter_.write(param.name(), param.value());
    }

    template <typename section_visitor_t>
    auto visit_section(std::string_view name, section_visitor_t&& section_visitor) const -> void
    {
        auto section_adapter = adapter_.create_section(name);
        std::invoke(std::forward<section_visitor_t>(section_visitor), writer_t{std::move(section_adapter)});
    }

private:
    adapter_t adapter_;
};

template <typename t_writer_t> struct writer_factory_t
{
    using writer_t = t_writer_t;

    auto create_writer(auto adapter) const -> writer_t { return writer_t{std::move(adapter)}; }
};

template <typename adapter_t> class reader_t
{
public:
    explicit reader_t(adapter_t adapter) noexcept : adapter_{std::move(adapter)} {}

    template <typename value_t, typename constraint_t>
    auto visit(reflection::param_t<value_t, constraint_t>& param) const -> void
    {
        value_t value;
        if (adapter_.read(param.name(), value)) param.value(std::move(value));
    }

    template <typename section_visitor_t>
    auto visit_section(std::string_view name, section_visitor_t&& section_visitor) const -> void
    {
        auto section_adapter = adapter_.get_section(name);
        if (section_adapter)
        {
            std::invoke(std::forward<section_visitor_t>(section_visitor), reader_t{*std::move(section_adapter)});
        }
    }

private:
    adapter_t adapter_;
};

template <typename t_reader_t> struct reader_factory_t
{
    using reader_t = t_reader_t;

    auto create_reader(auto adapter) const -> reader_t { return reader_t{std::move(adapter)}; }
};

template <typename archive_t, typename writer_factory_t> class serializer_t
{
public:
    explicit serializer_t(archive_t archive = {}, writer_factory_t writer_factory = {}) noexcept
        : archive_{std::move(archive)}, writer_factory_{std::move(writer_factory)}
    {}

    template <typename reflected_t>
    auto operator()(reflected_t const& obj, std::filesystem::path const& path) const -> void
    {
        serialize(obj, path);
    }

    template <typename reflected_t> auto operator()(reflected_t const& obj, std::ostream& out) const -> void
    {
        serialize(obj, out);
    }

private:
    template <typename reflected_t, typename out_t> auto serialize(reflected_t const& obj, out_t&& out) const -> void
    {
        auto document = archive_.create_write_document();
        auto writer = writer_factory_.create_writer(archive_.create_writer_adapter(document));
        obj.reflect(writer);
        archive_.write(document, std::forward<out_t>(out));
    }

    archive_t archive_;
    writer_factory_t writer_factory_;
};

template <typename archive_t, typename reader_factory_t> class deserializer_t
{
public:
    explicit deserializer_t(archive_t archive = {}, reader_factory_t reader_factory = {}) noexcept
        : archive_{std::move(archive)}, reader_factory_{std::move(reader_factory)}
    {}

    template <typename reflected_t> auto operator()(std::filesystem::path const& path, reflected_t& obj) const -> void
    {
        deserialize(path, obj);
    }

    template <typename reflected_t> auto operator()(std::istream& in, reflected_t& obj) const -> void
    {
        deserialize(in, obj);
    }

private:
    template <typename in_t, typename reflected_t> auto deserialize(in_t&& in, reflected_t& obj) const -> void
    {
        auto document = archive_.create_read_document(std::forward<in_t>(in));
        auto reader = reader_factory_.create_reader(archive_.create_reader_adapter(document));
        obj.reflect(reader);
    }

    archive_t archive_;
    reader_factory_t reader_factory_;
};

namespace {

//

} // namespace
} // namespace crv::serialization
