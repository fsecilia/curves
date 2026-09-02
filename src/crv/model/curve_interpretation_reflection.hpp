// SPDX-License-Identifier: MIT

/// \file
/// \brief adapts curve interpretation to model reflection
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/model/curve_interpretation.hpp>
#include <crv/reflection/enum.hpp>
#include <crv/sequential_enum_name_map.hpp>

namespace crv::reflection {

/// exposes authored curve interpretation through generic config reflection
template <> struct enum_t<model::curve_interpretation_t>
{
    static constexpr auto map = sequential_enum_name_map<model::curve_interpretation_t>("gain", "sensitivity");
};

} // namespace crv::reflection
