// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "archive.hpp"
#include <crv/ui/serialization/exceptions.hpp>
#include <crv/ui/serialization/toml/error_reporting.hpp>
#include <fstream>

namespace crv::serialization::tomlpp {

auto archive_t::read_document(std::filesystem::path const& path) const -> document_t
{
    try
    {
        return toml::parse_file(path.c_str());
    }
    catch (toml::parse_error const& exception)
    {
        report_error(exception.what(), exception.source());
    }
}

auto archive_t::read_document(std::istream& in) const -> document_t
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

auto archive_t::create_empty_document() const -> document_t
{
    return {};
}

auto archive_t::create_reader_adapter(document_t const& document) const -> reader_adapter_t
{
    return reader_adapter_t{document};
}

auto archive_t::create_writer_adapter(document_t& document) const -> writer_adapter_t
{
    return writer_adapter_t{document};
}

auto archive_t::write_document(document_t const& document, std::ostream& out) const -> void
{
    out << document;
    if (!out) throw io_x{"failed to write TOML document to output stream"};
}

auto archive_t::write_document(document_t const& document, std::filesystem::path const& path) const -> void
{
    auto out = std::ofstream{path};
    if (!out.is_open()) throw io_x{"failed to open file for writing: " + path.string()};

    out << document;
    if (!out) throw io_x{"failed to write TOML document to file: " + path.string()};
}

} // namespace crv::serialization::tomlpp
