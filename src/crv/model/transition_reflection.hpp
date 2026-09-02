// SPDX-License-Identifier: MIT

/// \file
/// \brief adapts transition continuity to model reflection
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/model/shaping/transitions/continuity.hpp>
#include <crv/reflection/enum.hpp>
#include <crv/sequential_enum_name_map.hpp>

namespace crv::reflection {

// temporary glue keeping transition math independent of reflection until transition dependencies are levelized
//
// This will need a real home in the future; it was roughed in to support getting curves::smooth_gain_t running.
template <> struct enum_t<shaping::transitions::continuity_t>
{
    static constexpr auto map
        = sequential_enum_name_map<shaping::transitions::continuity_t>("c1", "c2", "c3", "cinfinity");
};

} // namespace crv::reflection
