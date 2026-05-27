// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "config.hpp"
#include <crv/test/test.hpp>
#include <crv/ui/serialization/toml/toml.hpp>
#include <sstream>

namespace crv::model {
namespace {

TEST(model_test, round_trip)
{
    using serializer_t = serialization::tomlpp::serializer_t;
    using deserializer_t = serialization::tomlpp::deserializer_t;

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
