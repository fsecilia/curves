// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/concepts.hpp>
#include <crv/model/curves/curves.hpp>
#include <crv/model/curves/log_normal.hpp>
#include <crv/model/curves/synchronous.hpp>
#include <crv/reflection/constraints.hpp>
#include <crv/reflection/enum.hpp>
#include <crv/reflection/param.hpp>
#include <crv/tuple.hpp>
#include <string>
#include <tuple>

namespace crv::model {

using namespace reflection;
using namespace constraints;

/// soft limit for both x and y
constexpr auto soft_limit = 1e3;

using float_param_t = param_t<float_t, static_t<float_t, 1.0 / soft_limit, soft_limit>>;

struct device_t
{
    param_t<std::string> name{"Name", "Default"};
    param_t<int_t, static_lower_bound_t<int_t, 0>> dpi{"DPI", 0};
    param_t<float_t, static_t<float_t, -360.0 + 1e-3, 360.0 - 1e-3>> rotation{"Rotation (°)", 0.0};

    template <typename self_t, typename visitor_t>
    constexpr auto reflect(this self_t&& self, visitor_t&& visitor) -> decltype(auto)
    {
        visitor.visit(self.name);
        visitor.visit(self.dpi);
        visitor.visit(self.rotation);
        return std::forward<visitor_t>(visitor);
    }

    constexpr auto operator==(device_t const&) const noexcept -> bool = default;
};

struct offset_t
{
    param_t<float_t, static_lower_bound_t<float_t, 0.0>> begin{"Begin (c/ms)", 0.0};
    float_param_t width{"Width (c/ms)", 1.0};

    template <typename self_t, typename visitor_t>
    constexpr auto reflect(this self_t&& self, visitor_t&& visitor) -> decltype(auto)
    {
        visitor.visit(self.begin);
        visitor.visit(self.width);
        return std::forward<visitor_t>(visitor);
    }

    constexpr auto operator==(offset_t const&) const noexcept -> bool = default;
};

struct floor_t
{
    param_t<bool> enabled{"Enabled", false};
    param_t<float_t, static_t<float_t, 0.0, soft_limit>> anchor{"Min", 0.0};

    template <typename self_t, typename visitor_t>
    constexpr auto reflect(this self_t&& self, visitor_t&& visitor) -> decltype(auto)
    {
        visitor.visit(self.enabled);
        visitor.visit(self.anchor);
        return std::forward<visitor_t>(visitor);
    }

    constexpr auto operator==(floor_t const&) const noexcept -> bool = default;
};

struct limit_t
{
    param_t<float_t, static_t<float_t, 0.0, soft_limit>> max{"Max", soft_limit};
    float_param_t width{"Width (c/ms)", 1.0};

    template <typename self_t, typename visitor_t>
    constexpr auto reflect(this self_t&& self, visitor_t&& visitor) -> decltype(auto)
    {
        visitor.visit(self.max);
        visitor.visit(self.width);
        return std::forward<visitor_t>(visitor);
    }

    constexpr auto operator==(limit_t const&) const noexcept -> bool = default;
};

struct scale_t
{
    float_param_t input{"Input", 1.0};
    float_param_t output{"Output", 1.0};

    template <typename self_t, typename visitor_t>
    constexpr auto reflect(this self_t&& self, visitor_t&& visitor) -> decltype(auto)
    {
        visitor.visit(self.input);
        visitor.visit(self.output);
        return std::forward<visitor_t>(visitor);
    }

    constexpr auto operator==(scale_t const&) const noexcept -> bool = default;
};

struct common_curve_config_t
{
    scale_t scale;
    offset_t offset;
    floor_t floor;
    limit_t limit;

    template <typename self_t, typename visitor_t>
    constexpr auto reflect(this self_t&& self, visitor_t&& visitor) -> decltype(auto)
    {
        visitor.visit_section("Scale", [&](auto&& section_visitor) { self.scale.reflect(section_visitor); });
        visitor.visit_section("Offset", [&](auto&& section_visitor) { self.offset.reflect(section_visitor); });
        visitor.visit_section("Floor", [&](auto&& section_visitor) { self.floor.reflect(section_visitor); });
        visitor.visit_section("Limit", [&](auto&& section_visitor) { self.limit.reflect(section_visitor); });

        return std::forward<visitor_t>(visitor);
    }

    constexpr auto operator==(common_curve_config_t const&) const noexcept -> bool = default;
};

template <typename t_specific_curve_config_t> struct curve_config_t
{
    using specific_curve_config_t = t_specific_curve_config_t;
    common_curve_config_t common;
    specific_curve_config_t specific;

    template <typename self_t, typename visitor_t>
    constexpr auto reflect(this self_t&& self, visitor_t&& visitor) -> decltype(auto)
    {
        self.common.reflect(visitor);
        self.specific.reflect(visitor);
        return std::forward<visitor_t>(visitor);
    }

    constexpr auto operator==(curve_config_t const&) const noexcept -> bool = default;
};

template <typename curve_t> using extract_curve_config_t = curve_config_t<typename curve_t::config_t>;
using curve_configs_t = tuple::transform_t<curves::curves_t, extract_curve_config_t>;

struct profile_t
{
    param_t<curves::curve_id_t> active_curve{"Active Curve", curves::curve_id_t::synchronous};

    float_param_t anisotropy{"Anisotropy", 1.0};
    param_t<float_t, static_t<float_t, 0.0, 1000.0>> filter_halflife{"Filter Halflife (ms)", 2.0};
    param_t<float_t, static_t<float_t, 0.0, 1.0>> notch_width{"Notch Width (c/ms)", 0.0};

    curve_configs_t curve_configs;

    template <typename self_t, typename visitor_t>
    constexpr auto reflect(this self_t&& self, visitor_t&& visitor) -> decltype(auto)
    {
        visitor.visit(self.anisotropy);
        visitor.visit(self.filter_halflife);
        visitor.visit(self.notch_width);

        tuple::enumerate(self.curve_configs, [&](int_t id, auto&& curve_config) {
            auto const curve_id = static_cast<curves::curve_id_t>(id);
            visitor.visit_section(*reflection::to_string(curve_id),
                [&](auto&& section_visitor) { curve_config.reflect(section_visitor); });
        });

        return std::forward<visitor_t>(visitor);
    }

    constexpr auto operator==(profile_t const&) const noexcept -> bool = default;
};

struct root_t
{
    param_t<int_t, static_lower_bound_t<int_t, 0>> version{"Version", 1};

    device_t device;
    profile_t profile;

    template <typename self_t, typename visitor_t>
    constexpr auto reflect(this self_t&& self, visitor_t&& visitor) -> decltype(auto)
    {
        visitor.visit(self.version);

        visitor.visit_section("Default Device", [&](auto&& section_visitor) { self.device.reflect(section_visitor); });
        visitor.visit_section(
            "Default Profile", [&](auto&& section_visitor) { self.profile.reflect(section_visitor); });

        return std::forward<visitor_t>(visitor);
    }

    constexpr auto operator==(root_t const&) const noexcept -> bool = default;
};

} // namespace crv::model
