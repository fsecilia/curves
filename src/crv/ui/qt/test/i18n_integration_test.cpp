// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include <crv/test/test.hpp>
#include <crv/ui/qt/i18n.hpp>
#include <crv/ui/qt/test_app.hpp>
#include <QCoreApplication>
#include <QTranslator>

namespace crv::i18n::qt {
namespace {

struct i18n_qt_integration_test_t : Test
{
    test_app_t const& test_app = test_app_t::instance();
    QTranslator translator;

    provider_t const provider{};
    provider_i const* prev_provider = nullptr;

    auto SetUp() -> void override
    {
        ASSERT_TRUE(translator.load(":/i18n/test.qm")) << "failed to load test translation resource";
        QCoreApplication::installTranslator(&translator);

        prev_provider = &i18n::provider(provider);
    }

    auto TearDown() -> void override
    {
        if (prev_provider != nullptr) i18n::provider(*prev_provider);

        QCoreApplication::removeTranslator(&translator);
    }
};

TEST_F(i18n_qt_integration_test_t, tr_translates_known_string)
{
    auto const actual = CRV_TR("actual");

    EXPECT_EQ("expected", actual);
}

TEST_F(i18n_qt_integration_test_t, tr_falls_back_to_source_on_unknown_string)
{
    auto const actual = CRV_TR("<unknown>");

    EXPECT_EQ("<unknown>", actual);
}

TEST_F(i18n_qt_integration_test_t, tr_c_translates_known_string)
{
    auto const actual = CRV_TR_C("test", "test_actual");

    EXPECT_EQ("test_expected", actual);
}

TEST_F(i18n_qt_integration_test_t, tr_c_falls_back_to_source_on_unknown_string)
{
    auto const actual = CRV_TR_C("test", "<unknown>");

    EXPECT_EQ("<unknown>", actual);
}

} // namespace
} // namespace crv::i18n::qt
