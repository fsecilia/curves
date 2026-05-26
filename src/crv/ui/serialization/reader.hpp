// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/ui/reflection/enum.hpp>
#include <crv/ui/reflection/param.hpp>
#include <format>
#include <functional>
#include <string_view>
#include <utility>

namespace crv::serialization {

/// reads nested, typed, name/value pairs using adapter
template <typename adapter_t, typename error_reporter_t> class reader_t
{
public:
    reader_t(adapter_t adapter, error_reporter_t& reporter) : adapter_{std::move(adapter)}, reporter_{reporter} {}

    template <typename value_t, typename constraint_t>
    auto operator()(reflection::param_t<value_t, constraint_t>& param) -> void
    {
        auto value = param.value();
        adapter_.read(param.name(), value);
        param.value(std::move(value));
    }

    template <is_enum enum_t, typename constraint_t>
    auto operator()(reflection::param_t<enum_t, constraint_t>& param) -> void
    {
        auto value = std::string{};
        adapter_.read(param.name(), value);

        if (value.empty()) return;

        if (auto opt_enum = reflection::from_string<enum_t>(value)) param.value(*opt_enum);
        else reporter_.report_error(std::format("invalid value \"{}\" for enum \"{}\"", value, param.name()));
    }

    template <typename section_visitor_t>
    auto visit_section(std::string_view name, section_visitor_t&& section_visitor) -> void
    {
        if (auto section_adapter = adapter_.get_section(name))
        {
            // recurse with new reader wrapping nested adapter
            std::invoke(
                std::forward<section_visitor_t>(section_visitor), reader_t{*std::move(section_adapter), reporter_});
        }
    }

private:
    adapter_t adapter_;
    error_reporter_t& reporter_;
};

} // namespace crv::serialization
