// SPDX-License-Identifier: MIT

/// \file
/// \brief typed bools describing whether to saturate or not in a self-documenting way
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <type_traits>

namespace crv {

struct saturate : std::true_type
{};

struct no_saturation : std::false_type
{};

namespace detail {

template <typename saturation_t> struct literal_saturation_t : std::false_type
{};

template <> struct literal_saturation_t<saturate> : std::true_type
{};

template <> struct literal_saturation_t<no_saturation> : std::true_type
{};

template <typename saturation_t> inline constexpr auto literal_saturation_v = literal_saturation_t<saturation_t>::value;

} // namespace detail

template <typename saturation_t>
concept literal_saturation = detail::literal_saturation_v<saturation_t>;

} // namespace crv
