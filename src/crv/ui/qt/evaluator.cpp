// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "evaluator.hpp"
#include <cassert>

Q_DECLARE_METATYPE(crv::qt::evaluator_variant_t)

namespace crv::qt {

auto pack_evaluator(evaluator_variant_t const& evaluator) -> QVariant
{
    return QVariant::fromValue(evaluator);
}

auto unpack_evaluator(QVariant const& variant) -> evaluator_variant_t
{
    assert(variant.metaType().id() == qMetaTypeId<evaluator_variant_t>());
    return *reinterpret_cast<evaluator_variant_t const*>(variant.data());
}

} // namespace crv::qt
