// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "i18n.hpp"
#include <QCoreApplication>

namespace crv::i18n::qt {

auto provider_t::translate(char const* context, char const* source) const -> std::string
{
    return QCoreApplication::translate(context == nullptr ? "" : context, source == nullptr ? "" : source)
        .toStdString();
}

} // namespace crv::i18n::qt
