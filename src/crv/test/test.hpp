// SPDX-License-Identifier: MIT
/*!
  \file
  \brief main test header

  This file is included by every test file.

  \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <gtest/gtest.h>

namespace crv {

using namespace testing;

//! name generator for readable test output
template <typename test_vector_t> struct test_name_generator_t
{
    auto operator()(const TestParamInfo<test_vector_t>& info) const -> std::string
    {
        auto s = std::string{info.param.name};
        std::replace_if(s.begin(), s.end(), [](char c) { return !std::isalnum(c); }, '_');
        return s;
    }
};

} // namespace crv
