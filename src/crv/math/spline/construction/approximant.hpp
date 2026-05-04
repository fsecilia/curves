// SPDX-License-Identifier: MIT

/// \file
/// \brief interval approximant
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/jet/jet.hpp>
#include <crv/math/spline/segment.hpp>

namespace crv::spline {

template <typename real_t, typename segment_t> struct approximant_t
{
    using jet_t = jet_t<real_t>;
    jet_t left_knot;
    jet_t right_knot;

    segment_t polynomial;
};

} // namespace crv::spline
