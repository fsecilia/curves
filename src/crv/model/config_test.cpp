// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "config.hpp"
#include <crv/serialization/toml/toml.hpp>
#include <crv/test/test.hpp>
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

    // perturb expected
    expected_root.version.value(3);
    expected_root.device.name.value("name");
    expected_root.device.dpi.value(26000);
    expected_root.device.rotation.value(-1.1);
    expected_root.profile.anisotropy.value(5.1);
    expected_root.profile.filter_halflife.value(200);
    expected_root.profile.notch_width.value(0.5);

    auto& synchronous = std::get<curve_config_t<curves::synchronous_t::config_t>>(expected_root.profile.curve_configs);
    synchronous.common.scale.input.value(2.0);
    synchronous.common.scale.output.value(5.0);
    synchronous.common.offset.begin.value(1.0);
    synchronous.common.offset.width.value(1.5);
    synchronous.common.floor.mode.value(floor_mode_t::absolute);
    synchronous.common.floor.value.value(1.2);
    synchronous.common.limit.max.value(128.0);
    synchronous.common.limit.width.value(7.0);
    synchronous.specific.motivity.value(100.0);
    synchronous.specific.gamma.value(2.5);
    synchronous.specific.smooth.value(0.25);
    synchronous.specific.sync_speed.value(2.75);

    auto& log_normal = std::get<curve_config_t<curves::log_normal_t::config_t>>(expected_root.profile.curve_configs);
    log_normal.common.scale.input.value(6.0);
    log_normal.common.scale.output.value(9.0);
    log_normal.common.offset.begin.value(2.0);
    log_normal.common.offset.width.value(2.5);
    log_normal.common.floor.mode.value(floor_mode_t::absolute);
    log_normal.common.floor.value.value(3.2);
    log_normal.common.limit.max.value(256.0);
    log_normal.common.limit.width.value(11.0);
    log_normal.specific.center.value(10.0);
    log_normal.specific.width.value(5.0);

    // graphs no longer same
    EXPECT_NE(expected_root, actual_root);

    // round trip expected into actual
    auto out = std::ostringstream{};
    auto serializer = serializer_t{};
    serializer(expected_root, out);

    auto in = std::istringstream{out.str()};
    auto deserializer = deserializer_t{};
    deserializer(in, actual_root);

    // graphs are same again
    EXPECT_EQ(expected_root, actual_root);
};

} // namespace
} // namespace crv::model
