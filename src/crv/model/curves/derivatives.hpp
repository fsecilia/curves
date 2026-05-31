// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>

namespace crv::model::curves {

/// base function and derivatives up to order n
template <int n, typename scalar_t> struct derivatives_t;

template <typename scalar_t> struct derivatives_t<0, scalar_t>
{
    scalar_t f;
};

template <typename scalar_t> struct derivatives_t<1, scalar_t>
{
    scalar_t f, d1;
};

template <typename scalar_t> struct derivatives_t<2, scalar_t>
{
    scalar_t f, d1, d2;
};

template <typename scalar_t> struct derivatives_t<3, scalar_t>
{
    scalar_t f, d1, d2, d3;
};

} // namespace crv::model::curves
