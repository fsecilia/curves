// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/reflection/param.hpp>
#include <functional>
#include <string_view>
#include <utility>

namespace crv::serialization {

template <typename adapter_t> class writer_t
{
public:
    explicit writer_t(adapter_t adapter) : adapter_{std::move(adapter)} {}

    /// writes value under param's key
    ///
    /// \throws if value type is enum but content is not a valid member
    template <typename value_t, typename constraint_t>
    auto visit(reflection::param_t<value_t, constraint_t> const& param) -> void
    {
        adapter_.write(param.name(), param.value());
    }

    /// recurses visitor into nested section
    template <typename section_visitor_t>
    auto visit_section(std::string_view name, section_visitor_t&& section_visitor) -> void
    {
        auto section_adapter = adapter_.find_or_create_section(name);

        // recurse with new writer wrapping nested adapter
        std::invoke(std::forward<section_visitor_t>(section_visitor), writer_t{std::move(section_adapter)});
    }

private:
    adapter_t adapter_;
};

template <typename t_writer_t> struct writer_factory_t
{
    using writer_t = t_writer_t;

    auto create_writer(auto adapter) const -> writer_t { return writer_t{std::move(adapter)}; }
};

} // namespace crv::serialization
