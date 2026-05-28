// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/serialization/toml/reader_adapter.hpp>
#include <crv/serialization/toml/writer_adapter.hpp>
#include <filesystem>
#include <istream>
#include <ostream>
#include <toml++/toml.hpp>

namespace crv::serialization::tomlpp {

struct archive_t
{
    static constexpr auto file_extension = std::string_view{".toml"};

    using document_t = toml::table;

    auto create_empty_document() const -> document_t;
    auto read_document(std::filesystem::path const& path) const -> document_t;
    auto read_document(std::istream& in) const -> document_t;
    auto write_document(document_t const& document, std::ostream& out) const -> void;
    auto write_document(document_t const& document, std::filesystem::path const& path) const -> void;

    auto create_reader_adapter(document_t const& document) const -> reader_adapter_t;
    auto create_writer_adapter(document_t& document) const -> writer_adapter_t;
};

} // namespace crv::serialization::tomlpp
