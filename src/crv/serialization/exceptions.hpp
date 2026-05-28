// SPDX-License-Identifier: MIT

/// \file
/// \brief common exceptions thrown during serialization
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <stdexcept>

namespace crv::serialization {

struct serialization_x : std::runtime_error
{
    using std::runtime_error::runtime_error;
    virtual ~serialization_x() override;
};

struct format_x : serialization_x
{
    using serialization_x::serialization_x;
    virtual ~format_x() override;
};

struct io_x : serialization_x
{
    using serialization_x::serialization_x;
    virtual ~io_x() override;
};

struct parse_x : serialization_x
{
    using serialization_x::serialization_x;
    virtual ~parse_x() override;
};

} // namespace crv::serialization
