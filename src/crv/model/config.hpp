// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/concepts.hpp>
#include <crv/curves/curves.hpp>
#include <crv/curves/log_normal.hpp>
#include <crv/curves/synchronous.hpp>
#include <crv/reflection/constraints.hpp>
#include <crv/reflection/enum.hpp>
#include <crv/reflection/param.hpp>
#include <crv/tuple.hpp>
#include <string>

namespace crv {
namespace model {

using namespace reflection;
using namespace constraints;

/// soft limit for both x and y
constexpr auto soft_limit = 1e3;

using float_param_t = param_t<float_t, static_t<float_t, 1.0 / soft_limit, soft_limit>>;

struct device_t
{
    param_t<std::string> name{"name", "default"};
    param_t<int_t, static_lower_bound_t<int_t, 0>> dpi{"dpi", 0};
    param_t<float_t, static_t<float_t, -360.0 + 1e-3, 360.0 - 1e-3>> rotation{"rotation", 0.0};

    template <typename self_t, typename inspector_t>
    constexpr auto reflect(this self_t&& self, inspector_t&& inspector) -> decltype(auto)
    {
        inspector.inspect(self.name);
        inspector.inspect(self.dpi);
        inspector.inspect(self.rotation);
        return std::forward<inspector_t>(inspector);
    }

    constexpr auto operator==(device_t const&) const noexcept -> bool = default;
};

struct offset_t
{
    param_t<float_t, static_lower_bound_t<float_t, 0.0>> begin{"begin", 0.0};
    float_param_t width{"width", 1.0};

    template <typename self_t, typename inspector_t>
    constexpr auto reflect(this self_t&& self, inspector_t&& inspector) -> decltype(auto)
    {
        inspector.inspect(self.begin);
        inspector.inspect(self.width);
        return std::forward<inspector_t>(inspector);
    }

    constexpr auto operator==(offset_t const&) const noexcept -> bool = default;
};

enum class floor_mode_t
{
    absolute,
    relative,
};

struct floor_t
{
    param_t<floor_mode_t> mode{"mode", floor_mode_t::relative};
    param_t<float_t, static_t<float_t, -soft_limit, soft_limit>> value{"value", 0.0};

    template <typename self_t, typename inspector_t>
    constexpr auto reflect(this self_t&& self, inspector_t&& inspector) -> decltype(auto)
    {
        inspector.inspect(self.mode);
        inspector.inspect(self.value);
        return std::forward<inspector_t>(inspector);
    }

    constexpr auto operator==(floor_t const&) const noexcept -> bool = default;
};

struct limit_t
{
    param_t<float_t, static_t<float_t, 0.0, soft_limit>> max{"maximum", soft_limit};
    float_param_t width{"width", 1.0};

    template <typename self_t, typename inspector_t>
    constexpr auto reflect(this self_t&& self, inspector_t&& inspector) -> decltype(auto)
    {
        inspector.inspect(self.max);
        inspector.inspect(self.width);
        return std::forward<inspector_t>(inspector);
    }

    constexpr auto operator==(limit_t const&) const noexcept -> bool = default;
};

struct scale_t
{
    float_param_t input{"input", 1.0};
    float_param_t output{"output", 1.0};

    template <typename self_t, typename inspector_t>
    constexpr auto reflect(this self_t&& self, inspector_t&& inspector) -> decltype(auto)
    {
        inspector.inspect(self.input);
        inspector.inspect(self.output);
        return std::forward<inspector_t>(inspector);
    }

    constexpr auto operator==(scale_t const&) const noexcept -> bool = default;
};

struct common_curve_config_t
{
    scale_t scale;
    offset_t offset;
    floor_t floor;
    limit_t limit;

    template <typename self_t, typename inspector_t>
    constexpr auto reflect(this self_t&& self, inspector_t&& inspector) -> decltype(auto)
    {
        inspector.inspect_section("scale", [&](auto&& section_inspector) { self.scale.reflect(section_inspector); });
        inspector.inspect_section("offset", [&](auto&& section_inspector) { self.offset.reflect(section_inspector); });
        inspector.inspect_section("floor", [&](auto&& section_inspector) { self.floor.reflect(section_inspector); });
        inspector.inspect_section("limit", [&](auto&& section_inspector) { self.limit.reflect(section_inspector); });

        return std::forward<inspector_t>(inspector);
    }

    constexpr auto operator==(common_curve_config_t const&) const noexcept -> bool = default;
};

template <typename t_specific_curve_config_t> struct curve_config_t
{
    using specific_curve_config_t = t_specific_curve_config_t;
    common_curve_config_t common;
    specific_curve_config_t specific;

    template <typename self_t, typename inspector_t>
    constexpr auto reflect(this self_t&& self, inspector_t&& inspector) -> decltype(auto)
    {
        self.common.reflect(inspector);
        self.specific.reflect(inspector);
        return std::forward<inspector_t>(inspector);
    }

    constexpr auto operator==(curve_config_t const&) const noexcept -> bool = default;
};

template <typename curve_t> using extract_curve_config_t = curve_config_t<typename curve_t::config_t>;
using curve_configs_t = tuple::transform_t<curves::curves_t, extract_curve_config_t>;

struct profile_t
{
    float_param_t anisotropy{"anisotropy", 1.0};
    param_t<float_t, static_t<float_t, 0.0, 1000.0>> filter_halflife{"filter_halflife", 2.0};

    param_t<curves::curve_id_t> active_curve{"active_curve", curves::curve_id_t::synchronous};
    curve_configs_t curve_configs;

    template <typename self_t, typename inspector_t>
    constexpr auto reflect(this self_t&& self, inspector_t&& inspector) -> decltype(auto)
    {
        inspector.inspect(self.anisotropy);
        inspector.inspect(self.filter_halflife);

        inspector.inspect(self.active_curve);
        inspector.inspect_section("curves", [&]([[maybe_unused]] auto&& curves_inspector) {
            tuple::enumerate(self.curve_configs, [&](int_t id, auto&& curve_config) {
                auto const curve_id = static_cast<curves::curve_id_t>(id);
                inspector.inspect_section(*reflection::to_string(curve_id),
                    [&](auto&& section_inspector) { curve_config.reflect(section_inspector); });
            });
        });

        return std::forward<inspector_t>(inspector);
    }

    constexpr auto operator==(profile_t const&) const noexcept -> bool = default;
};

struct root_t
{
    param_t<int_t, static_lower_bound_t<int_t, 0>> version{"version", 1};

    param_t<std::string> active_device{"active_device", "default_device"};
    device_t device;

    param_t<std::string> active_profile{"active_profile", "default_profile"};
    profile_t profile;

    template <typename self_t, typename inspector_t>
    constexpr auto reflect(this self_t&& self, inspector_t&& inspector) -> decltype(auto)
    {
        inspector.inspect(self.version);

        inspector.inspect(self.active_device);
        inspector.inspect_section(
            self.active_device.value(), [&](auto&& section_inspector) { self.device.reflect(section_inspector); });

        inspector.inspect(self.active_profile);
        inspector.inspect_section(
            self.active_profile.value(), [&](auto&& section_inspector) { self.profile.reflect(section_inspector); });

        return std::forward<inspector_t>(inspector);
    }

    constexpr auto operator==(root_t const&) const noexcept -> bool = default;
};

} // namespace model

namespace reflection {

template <> struct enum_t<model::floor_mode_t>
{
    static constexpr auto map = sequential_enum_name_map<model::floor_mode_t>("absolute", "relative");
};

} // namespace reflection

} // namespace crv
