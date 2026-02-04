// SPDX-License-Identifier: MIT
/*!
    \file
    \copyright Copyright (C) 2026 Frank Secilia
*/

#include "float128.hpp"
#include <array>
#include <quadmath.h>

namespace std {

#if defined CRV_POLYFILL_FLOAT128_OSTREAM_INSERTER
auto operator<<(std::ostream& out, float128_t src) -> std::ostream&
{
    // determine format specifier from stream flags
    auto       specifier = 'g';
    auto const flags     = out.flags();
    if (flags & std::ios::fixed) { specifier = 'f'; }
    else if (flags & std::ios::scientific) { specifier = 'e'; }

    // build format string dynamically based on specifier and precision
    std::array<char, 64> format;
    auto const           precision     = std::max(0ll, static_cast<long long>(out.precision()));
    auto const           format_length = std::snprintf(format.data(), format.size(), //
                                                       "%%.%lldQ%c", precision, specifier);

    // if there's something wrong the the format string or precision, use a default
    if (format_length < 0 || static_cast<size_t>(format_length) >= format.size())
    {
        std::snprintf(format.data(), format.size(), "%%.6Qg");
    }

    // write float using format string
    std::array<char, 128> buffer;
    auto const            intended_length = quadmath_snprintf(buffer.data(), buffer.size(), format.data(), src);
    auto const            actual_length   = std::min(static_cast<size_t>(intended_length), buffer.size() - 1);
    if (actual_length > 0) out.write(buffer.data(), actual_length);

    return out;
}
#endif

} // namespace crv
