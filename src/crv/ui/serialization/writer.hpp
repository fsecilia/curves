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

template <typename adapter_t, typename error_reporter_t> class writer_t
{
public:
    writer_t(adapter_t adapter, error_reporter_t& reporter) : adapter_{std::move(adapter)}, reporter_{reporter} {}

    /// writes generic value under param's key
    template <typename value_t, typename constraint_t>
    auto operator()(reflection::param_t<value_t, constraint_t> const& param) -> void
    {
        adapter_.write(param.name(), param.value());
    }

    /// writes enum value under param's key; reports error if enum is invalid
    template <is_enum value_t, typename constraint_t>
    auto operator()(reflection::param_t<value_t, constraint_t> const& param) -> void
    {
        if (auto const opt_enum = reflection::to_string(param.value())) adapter_.write(param.name(), *opt_enum);
        else reporter_.report_error(std::format("unmapped value for enum \"{}\"", param.name()));
    }

    /// recurses visitor into nested section
    template <typename section_visitor_t>
    auto visit_section(std::string_view name, section_visitor_t&& section_visitor) -> void
    {
        auto section_adapter = adapter_.create_section(name);

        // recurse with new writer wrapping nested adapter
        std::invoke(std::forward<section_visitor_t>(section_visitor), writer_t{std::move(section_adapter), reporter_});
    }

private:
    adapter_t adapter_;
    error_reporter_t& reporter_;
};

} // namespace crv::serialization
