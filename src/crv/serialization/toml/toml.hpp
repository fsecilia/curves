// SPDX-License-Identifier: MIT

/// \file
/// \brief toml++ facade
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/serialization/deserializer.hpp>
#include <crv/serialization/reader.hpp>
#include <crv/serialization/serializer.hpp>
#include <crv/serialization/toml/archive.hpp>
#include <crv/serialization/toml/reader_adapter.hpp>
#include <crv/serialization/toml/writer_adapter.hpp>
#include <crv/serialization/writer.hpp>

namespace crv::serialization::tomlpp {

using reader_t = serialization::reader_t<reader_adapter_t>;
using reader_factory_t = serialization::reader_factory_t<reader_t>;
using deserializer_t = serialization::deserializer_t<archive_t, reader_factory_t>;

using writer_t = serialization::writer_t<writer_adapter_t>;
using writer_factory_t = serialization::writer_factory_t<writer_t>;
using serializer_t = serialization::serializer_t<archive_t, writer_factory_t>;

} // namespace crv::serialization::tomlpp
