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
    expected_root.profile.output_dpi.value(1600);
    expected_root.profile.anisotropy.value(5.1);
    expected_root.profile.filter_halflife.value(200);

    auto& synchronous = std::get<curve_config_t<curves::synchronous_t::config_t>>(expected_root.profile.curves.configs);
    synchronous.common.scale.input.value(2.0);
    synchronous.common.scale.output.value(5.0);
    synchronous.common.offset.begin.value(1.0);
    synchronous.common.offset.width.value(1.5);
    synchronous.common.anchor.mode.value(anchor_mode_t::fixed);
    synchronous.common.anchor.height.value(1.2);
    synchronous.common.ceiling.height.value(128.0);
    synchronous.common.ceiling.width.value(7.0);
    synchronous.specific.motivity.value(100.0);
    synchronous.specific.gamma.value(2.5);
    synchronous.specific.smooth.value(0.25);
    synchronous.specific.sync_speed.value(2.75);

    auto& log_normal = std::get<curve_config_t<curves::log_normal_t::config_t>>(expected_root.profile.curves.configs);
    log_normal.common.scale.input.value(6.0);
    log_normal.common.scale.output.value(9.0);
    log_normal.common.offset.begin.value(2.0);
    log_normal.common.offset.width.value(2.5);
    log_normal.common.anchor.mode.value(anchor_mode_t::fixed);
    log_normal.common.anchor.height.value(3.2);
    log_normal.common.ceiling.height.value(256.0);
    log_normal.common.ceiling.width.value(11.0);
    log_normal.specific.baseline.value(5.0);
    log_normal.specific.limit.value(10.0);
    log_normal.specific.accel_peak.value(7.0);
    log_normal.specific.max_accel.value(11.0);

    auto& smooth_gain = std::get<curve_config_t<curves::smooth_gain_t::config_t>>(expected_root.profile.curves.configs);
    smooth_gain.common.scale.input.value(3.0);
    smooth_gain.common.scale.output.value(7.0);
    smooth_gain.common.offset.begin.value(4.0);
    smooth_gain.common.offset.width.value(3.5);
    smooth_gain.common.anchor.mode.value(anchor_mode_t::fixed);
    smooth_gain.common.anchor.height.value(4.2);
    smooth_gain.common.ceiling.height.value(512.0);
    smooth_gain.common.ceiling.width.value(13.0);
    smooth_gain.specific.v_0.value(-1.0);
    smooth_gain.specific.v_1.value(12.0);
    smooth_gain.specific.g_t.value(0.75);
    smooth_gain.specific.g_f.value(3.0);
    smooth_gain.specific.transition.value(shaping::transitions::continuity_t::c2);

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

TEST(model_transition_reflection_test_t, names_continuity)
{
    EXPECT_EQ(reflection::to_string(shaping::transitions::continuity_t::cinfinity), "cinfinity");
}

TEST(model_transition_reflection_test_t, parses_continuity)
{
    EXPECT_EQ(
        reflection::from_string<shaping::transitions::continuity_t>("c2"), shaping::transitions::continuity_t::c2);
}

} // namespace
} // namespace crv::model
