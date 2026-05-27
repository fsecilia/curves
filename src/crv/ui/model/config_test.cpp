// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "config.hpp"
#include <crv/test/test.hpp>
#include <crv/ui/serialization/deserializer.hpp>
#include <crv/ui/serialization/reader.hpp>
#include <crv/ui/serialization/serializer.hpp>
#include <crv/ui/serialization/toml/archive.hpp>
#include <crv/ui/serialization/toml/reader_adapter.hpp>
#include <crv/ui/serialization/toml/writer_adapter.hpp>
#include <crv/ui/serialization/writer.hpp>
#include <sstream>

namespace crv::model {
namespace {

TEST(model_test, round_trip)
{
    using archive_t = serialization::tomlpp::archive_t;

    using writer_adapter_t = serialization::tomlpp::writer_adapter_t;
    using writer_t = serialization::writer_t<writer_adapter_t>;
    using writer_factory_t = serialization::writer_factory_t<writer_t>;
    using serializer_t = serialization::serializer_t<archive_t, writer_factory_t>;

    using reader_adapter_t = serialization::tomlpp::reader_adapter_t;
    using reader_t = serialization::reader_t<reader_adapter_t>;
    using reader_factory_t = serialization::reader_factory_t<reader_t>;
    using deserializer_t = serialization::deserializer_t<archive_t, reader_factory_t>;

    auto expected_root = root_t{};
    auto actual_root = root_t{};

    // graphs initially same
    EXPECT_EQ(expected_root, actual_root);

    // perturb all of expected
    expected_root.version.value(3);
    expected_root.device.name.value("name");
    expected_root.device.id.value("id");
    expected_root.device.dpi.value(26000);
    expected_root.device.rotation.value(-1.1);
    expected_root.profile.anisotropy.value(5.1);
    expected_root.profile.filter_halflife.value(200);
    expected_root.profile.notch_width.value(0.5);

    // graphs no longer same
    EXPECT_NE(expected_root, actual_root);

    // round trip expected into actual
    auto out = std::ostringstream{};
    auto serializer = serializer_t{};
    serializer(expected_root, out);

    std::cout << out.str() << std::endl;

    auto in = std::istringstream{out.str()};
    auto deserializer = deserializer_t{};
    deserializer(in, actual_root);

    // graphs are same again
    EXPECT_EQ(expected_root, actual_root);
};

} // namespace
} // namespace crv::model
