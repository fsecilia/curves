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
template <typename callback_t> class hierarchical_inspector_t
{
public:
    explicit hierarchical_inspector_t(callback_t callback) : callback_{std::move(callback)} {}

    auto inspect(this auto&& self, auto&& param) -> void
    {
        auto const full_path = self.current_path_.empty() ? std::string(param.name())
                                                          : self.current_path_ + "/" + std::string(param.name());

        std::forward<decltype(self)>(self).callback_(full_path, param);
    }

    template <typename section_inspector_t>
    auto inspect_section(std::string_view name, section_inspector_t&& section_inspector) -> void
    {
        auto old_path = current_path_;

        current_path_ = current_path_.empty() ? std::string(name) : current_path_ + "/" + std::string(name);

        std::invoke(std::forward<section_inspector_t>(section_inspector), *this);

        current_path_ = std::move(old_path);
    }

private:
    callback_t callback_;
    std::string current_path_;
};

} // namespace crv
