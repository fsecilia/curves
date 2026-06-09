// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <functional>
#include <string>
#include <utility>

namespace crv {

/// inspector that builds hierarchical keys for property mapping
template <typename callback_t, typename predicate_t> class hierarchical_inspector_t
{
public:
    constexpr hierarchical_inspector_t(callback_t callback, predicate_t predicate)
        : callback_{std::move(callback)}, predicate_{std::move(predicate)}
    {}

    constexpr auto inspect(this auto&& self, auto&& param) -> void
    {
        auto const full_path = self.current_path_.empty() ? std::string(param.name())
                                                          : self.current_path_ + "/" + std::string(param.name());

        std::forward<decltype(self)>(self).callback_(full_path, param);
    }

    template <typename section_inspector_t>
    constexpr auto inspect_section(std::string_view name, section_inspector_t&& section_inspector) -> void
    {
        // filter nested path via predicate
        auto nested_path = current_path_.empty() ? std::string(name) : current_path_ + "/" + std::string(name);
        if (!predicate_(nested_path)) return;

        // descend
        auto old_path = std::move(current_path_);
        current_path_ = std::move(nested_path);

        // invoke
        std::invoke(std::forward<section_inspector_t>(section_inspector), *this);

        // restore
        current_path_ = std::move(old_path);
    }

private:
    callback_t callback_;
    predicate_t predicate_;
    std::string current_path_;
};

struct hierarchical_inspector_factory_t
{
    template <typename callback_t, typename predicate_t>
    constexpr auto operator()(callback_t callback, predicate_t predicate) const noexcept
        -> hierarchical_inspector_t<callback_t, predicate_t>
    {
        return hierarchical_inspector_t{std::move(callback), std::move(predicate)};
    }
};

} // namespace crv
