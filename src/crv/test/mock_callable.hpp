// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/test/test.hpp>
#include <gmock/gmock.h>

namespace crv {

template <typename return_t, typename arg_t> struct mock_callable_t
{
    virtual ~mock_callable_t() = default;

    MOCK_METHOD(return_t, call_const, (arg_t), (const, noexcept));
    MOCK_METHOD(return_t, call, (arg_t), (noexcept));
};

template <typename return_t, typename arg_t> struct callable_t
{
    mock_callable_t<return_t, arg_t>* mock = nullptr;

    auto operator()(arg_t arg) const noexcept -> return_t { return mock->call_const(arg); }
    auto operator()(arg_t arg) noexcept -> return_t { return mock->call(arg); }
};

} // namespace crv
