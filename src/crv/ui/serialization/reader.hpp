// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/ui/reflection/param.hpp>
#include <functional>
#include <string_view>
#include <utility>

namespace crv::serialization {

/// reads nested, typed, name/value pairs using adapter
template <typename adapter_t> class reader_t
{
public:
    explicit reader_t(adapter_t adapter) : adapter_{std::move(adapter)} {}

    /// reads value under param's key; does nothing if not present, reports error if types do not match
    template <typename value_t, typename constraint_t>
    auto operator()(reflection::param_t<value_t, constraint_t>& param) -> void
    {
        auto value = param.value();
        if (!adapter_.read(param.name(), value)) return;
        param.value(std::move(value));
    }

    /// recurses visitor into nested section if found; does nothing if not present, reports error if not a section
    template <typename section_visitor_t>
    auto visit_section(std::string_view name, section_visitor_t&& section_visitor) -> void
    {
        if (auto section_adapter = adapter_.get_section(name))
        {
            // recurse with new reader wrapping nested adapter
            std::invoke(std::forward<section_visitor_t>(section_visitor), reader_t{*std::move(section_adapter)});
        }
    }

private:
    adapter_t adapter_;
};

} // namespace crv::serialization
