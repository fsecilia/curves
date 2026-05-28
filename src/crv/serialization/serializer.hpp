// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <filesystem>
#include <ostream>
#include <utility>

namespace crv::serialization {

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
        auto document = archive_.create_empty_document();
        auto writer = writer_factory_.create_writer(archive_.create_writer_adapter(document));
        obj.reflect(writer);
        archive_.write_document(document, std::forward<out_t>(out));
    }

    archive_t archive_;
    writer_factory_t writer_factory_;
};

} // namespace crv::serialization
