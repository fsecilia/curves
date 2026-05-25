// SPDX-License-Identifier: MIT

/// \file
/// \brief common exceptions thrown during serialization
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <stdexcept>

namespace crv::serialization {

struct parse_x : std::runtime_error
{
    using std::runtime_error::runtime_error;
    virtual ~parse_x() override;
};

} // namespace crv::serialization
