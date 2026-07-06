// SPDX-License-Identifier: MIT

/// \file
/// \brief qt i18n provider implementation
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/i18n.hpp>
#include <crv/ui/qt/lib.hpp>

namespace crv::i18n::qt {

class qt_ui_api provider_t final : public provider_i
{
public:
    auto translate(char const* context, char const* source) const -> std::string override;
};

} // namespace crv::i18n::qt
