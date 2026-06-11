// SPDX-License-Identifier: MIT

/// \file
/// \brief list of curves and curve names
///
/// This file is the source of truth of supported curves. When adding a new curve, add an entry for it to curve_type_t,
/// add its type to curves_t, and name it in the map. All 3 of these must be in the same order.
///
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/curves/log_normal.hpp>
#include <crv/curves/synchronous.hpp>
#include <crv/reflection/enum.hpp>
#include <crv/sequential_enum_name_map.hpp>
#include <tuple>

namespace crv {
namespace model::curves {

/// integer values to identify each curve
///
/// *** Order here must match curves_t! ***
///
/// Serialization depends on these concrete values. This list is append-only; do not reorder.
enum class curve_id_t
{
    synchronous,
    log_normal,
    count_
};

/// curve types, ordered by id
///
/// *** Order here must match curve_id_t! ***
using curves_t = std::tuple<synchronous_t, log_normal_t>;
constexpr auto curves_count = static_cast<int_t>(std::tuple_size_v<curves_t>);

} // namespace model::curves

namespace reflection {

template <> struct enum_t<model::curves::curve_id_t>
{
    static constexpr auto map = sequential_enum_name_map<model::curves::curve_id_t>("synchronous", "log_normal");
};

} // namespace reflection

namespace model::curves {

// make sure all entries have the same number of entries; order can't be enforced programmatically
static_assert(curves_count == static_cast<std::size_t>(curve_id_t::count_));
static_assert(curves_count == reflection::enum_t<curve_id_t>::map.size());

} // namespace model::curves
} // namespace crv
