// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <filesystem>
#include <istream>
#include <utility>

namespace crv::serialization {

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
        auto const document = archive_.read_document(std::forward<in_t>(in));
        auto reader = reader_factory_.create_reader(archive_.create_reader_adapter(document));
        obj.reflect(reader);
    }

    archive_t archive_;
    reader_factory_t reader_factory_;
};

} // namespace crv::serialization
