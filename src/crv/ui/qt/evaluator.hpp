// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/curves/evaluator.hpp>
#include <crv/ui/qt/lib.hpp>
#include <QVariant>

namespace crv::qt {

using evaluator_variant_t = model::curves::evaluator_variant_t<float_t>;

qt_ui_api auto pack_evaluator(evaluator_variant_t const& evaluator) -> QVariant;

/// \pre variant contains an evaluator_variant_t
qt_ui_api auto unpack_evaluator(QVariant const& variant) -> evaluator_variant_t;

} // namespace crv::qt
