// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/model/shaped_curve.hpp>
#include <crv/ui/qt/lib.hpp>
#include <QVariant>

namespace crv::qt {

using composed_curve_variant_t = model::curves::composed_curve_variant_t<float_t>;

qt_ui_api auto pack_curve(composed_curve_variant_t const& composed_curve) -> QVariant;

/// \pre variant contains valid value
qt_ui_api auto unpack_curve(QVariant const& variant) -> composed_curve_variant_t;

} // namespace crv::qt
