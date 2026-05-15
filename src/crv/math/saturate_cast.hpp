// SPDX-License-Identifier: MIT

/// \file
/// \brief std::saturate_cast polyfill
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <version>

#if defined(__cpp_lib_saturation) && __cpp_lib_saturation >= 202311L
#include <numeric>
#else
#include <crv/math/limits.hpp>
#include <concepts>
#include <utility>
#endif

namespace crv {

#if defined(__cpp_lib_saturation) && __cpp_lib_saturation >= 202311L

using std::saturate_cast;

#else

/// casts between integer types, clamping to the representable range of the destination type
///
/// \tparam dst_t destination integer type
/// \tparam src_t source integer type
/// \param src value to cast
/// \return saturated src
template <typename dst_t, typename src_t>
    requires std::integral<dst_t> && std::integral<src_t> && (!std::same_as<std::remove_cv_t<dst_t>, bool>)
    && (!std::same_as<std::remove_cv_t<src_t>, bool>)
[[nodiscard]] constexpr dst_t saturate_cast(src_t src) noexcept
{
    if (std::cmp_greater(src, max<dst_t>())) return max<dst_t>();
    if (std::cmp_less(src, min<dst_t>())) return min<dst_t>();
    return static_cast<dst_t>(src);
}

#endif

} // namespace crv
