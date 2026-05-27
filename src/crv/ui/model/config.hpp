// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/concepts.hpp>
#include <crv/sequential_enum_name_map.hpp>
#include <crv/ui/reflection/constraints.hpp>
#include <crv/ui/reflection/enum.hpp>
#include <crv/ui/reflection/param.hpp>
#include <string>

namespace crv::model {

using namespace reflection;
using namespace constraints;

using float_param_t = param_t<float_t, static_t<float_t, 1e-3, 1e3>>;

struct device_t
{
    param_t<std::string> name{"Name", "Default"};
    param_t<int_t> dpi{"DPI", 0};
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
    param_t<float_t> begin{"Begin (c/ms)", 0.0};
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

struct limit_t
{
    param_t<float_t> limit{"Limit", 1000.0};
    float_param_t width{"Width (c/ms)", 1.0};

    template <typename self_t, typename visitor_t>
    constexpr auto reflect(this self_t&& self, visitor_t&& visitor) -> decltype(auto)
    {
        visitor.visit(self.limit);
        visitor.visit(self.width);
        return std::forward<visitor_t>(visitor);
    }

    constexpr auto operator==(limit_t const&) const noexcept -> bool = default;
};

struct common_curve_config_t
{
    float_param_t input_scale{"Input Scale", 1.0};
    float_param_t output_scale{"Output Scale", 1.0};

    offset_t offset;
    limit_t limit;

    template <typename self_t, typename visitor_t>
    constexpr auto reflect(this self_t&& self, visitor_t&& visitor) -> decltype(auto)
    {
        visitor.visit(self.input_scale);
        visitor.visit(self.output_scale);

        visitor.visit_section("Offset", [&](auto&& section_visitor) { self.offset.reflect(section_visitor); });
        visitor.visit_section("Limit", [&](auto&& section_visitor) { self.limit.reflect(section_visitor); });

        return std::forward<visitor_t>(visitor);
    }

    constexpr auto operator==(common_curve_config_t const&) const noexcept -> bool = default;
};

template <typename specific_curve_config_t> struct curve_config_t
{
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

struct profile_t
{
    float_param_t anisotropy{"Y/X scaling", 1.0};
    param_t<float_t, static_t<float_t, 0.0, 1000.0>> filter_halflife{"Filter Halflife (ms)", 2.0};
    param_t<float_t, static_t<float_t, 0.0, 1.0>> notch_width{"Notch Width (c/ms)", 0.0};

    // curve configs

    template <typename self_t, typename visitor_t>
    constexpr auto reflect(this self_t&& self, visitor_t&& visitor) -> decltype(auto)
    {
        visitor.visit(self.anisotropy);
        visitor.visit(self.filter_halflife);
        visitor.visit(self.notch_width);
        return std::forward<visitor_t>(visitor);
    }

    constexpr auto operator==(profile_t const&) const noexcept -> bool = default;
};

struct root_t
{
    param_t<int_t> version{"Version", 1};

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
