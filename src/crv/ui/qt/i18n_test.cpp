// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "i18n.hpp"
#include <crv/test/test.hpp>

namespace crv::i18n::qt {
namespace {

struct qt_i18n_test_t : Test
{
    provider_t const provider{};
    scoped_provider_t const scoped_provider{provider};

    char const* expected = "expected";
};

TEST_F(qt_i18n_test_t, crv_tr_c_returns_source_when_no_translator_loaded)
{
    auto const result = CRV_TR_C("context", expected);

    EXPECT_EQ(std::string{expected}, result);
}

TEST_F(qt_i18n_test_t, crv_tr_returns_source_when_no_translator_loaded)
{
    auto const result = CRV_TR(expected);

    EXPECT_EQ(std::string{expected}, result);
}

} // namespace
} // namespace crv::i18n::qt
