// SPDX-License-Identifier: MIT

/// \file
/// \brief standard overloaded idiom
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>

namespace crv {

/// standard overloaded idiom
///
/// This type aggregates and delegates to an overload set. Its primary purpose is to dispatch a variant to a
/// type-specific overload from the set.
template <typename... overloads_t> struct overloaded_t : overloads_t...
{
    using overloads_t::operator()...;
};

} // namespace crv
