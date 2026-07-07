// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "packed_curve.hpp"
#include <cassert>

Q_DECLARE_METATYPE(crv::qt::curve_variant_t)

namespace crv::qt {

auto pack_curve(curve_variant_t const& curve) -> QVariant
{
    return QVariant::fromValue(curve);
}

auto unpack_curve(QVariant const& variant) -> curve_variant_t
{
    assert(variant.metaType().id() == qMetaTypeId<curve_variant_t>());
    return *reinterpret_cast<curve_variant_t const*>(variant.data());
}

} // namespace crv::qt
