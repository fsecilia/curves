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

namespace crv {

using namespace reflection;
using namespace constraints;

using float_param_t = param_t<float_t, static_t<float_t, 1e-3, 1e3>>;

struct device_t
{
    param_t<std::string> name{"Name", "<device-name>"};
    param_t<std::string> id{"Id", "<device-id>"};
    param_t<int_t> dpi{"DPI", 0};
    param_t<float_t, static_t<float_t, -360.0 + 1e-3, 360.0 - 1e-3>> rotation{"rotation (°)", 0.0};

    template <typename self_t, typename visitor_t>
    constexpr auto reflect(this self_t&& self, visitor_t&& visitor) -> decltype(auto)
    {
        visitor.visit(self.name);
        visitor.visit(self.id);
        visitor.visit(self.dpi);
        visitor.visit(self.rotation);
        return std::forward<visitor_t>(visitor);
    }
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

        visitor.visit_section("offset", [&](auto&& section_visitor) { self.offset.reflect(section_visitor); });
        visitor.visit_section("limit", [&](auto&& section_visitor) { self.limit.reflect(section_visitor); });

        return std::forward<visitor_t>(visitor);
    }
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
};

struct profile_t
{
    float_param_t anisotropy{"y/x scaling", 1.0};
    param_t<float_t, static_t<float_t, 0.0, 1000.0>> filter_halflife{"filter halflife (ms)", 2.0};
    param_t<float_t, static_t<float_t, 0.0, 1.0>> notch_width{"notch_width (c/ms)", 0.0};

    // curve configs

    template <typename self_t, typename visitor_t>
    constexpr auto reflect(this self_t&& self, visitor_t&& visitor) -> decltype(auto)
    {
        visitor(self.anisotropy);
        visitor(self.filter_halflife);
        visitor(self.notch_width);
        return std::forward<visitor_t>(visitor);
    }
};

struct root_t
{
    param_t<int_t> version{"version", 1};

    profile_t profile;

    template <typename self_t, typename visitor_t>
    constexpr auto reflect(this self_t&& self, visitor_t&& visitor) -> decltype(auto)
    {
        visitor(self.version);

        visitor.visit_section(
            "default profile", [&](auto&& section_visitor) { self.profile.reflect(section_visitor); });

        return std::forward<visitor_t>(visitor);
    }
};

} // namespace crv
