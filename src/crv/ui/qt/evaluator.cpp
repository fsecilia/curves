// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "evaluator.hpp"
#include <cassert>

Q_DECLARE_METATYPE(crv::qt::composed_curve_variant_t)

namespace crv::qt {

auto pack_curve(composed_curve_variant_t const& composed_curve) -> QVariant
{
    return QVariant::fromValue(composed_curve);
}

auto unpack_curve(QVariant const& variant) -> composed_curve_variant_t
{
    assert(variant.metaType().id() == qMetaTypeId<composed_curve_variant_t>());
    return *reinterpret_cast<composed_curve_variant_t const*>(variant.data());
}

} // namespace crv::qt
