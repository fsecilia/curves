// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "i18n.hpp"
#include <crv/test/test.hpp>
#include <gmock/gmock.h>

namespace crv::i18n {
namespace {

struct i18n_test_t : Test
{
    static constexpr char const* context = "context";
    static constexpr char const* source = "source";
};

//
// providers
//

struct i18n_identity_provider_test_t : i18n_test_t
{
    using sut_t = identity_provider_t;
    sut_t sut{};
};

TEST_F(i18n_identity_provider_test_t, source_returned_verbatim)
{
    auto const actual = sut.translate(context, source);

    EXPECT_EQ(source, actual);
}

TEST_F(i18n_identity_provider_test_t, unaffected_by_context)
{
    auto const actual = sut.translate("", source);

    EXPECT_EQ(source, actual);
}

TEST_F(i18n_identity_provider_test_t, empty_source)
{
    auto const actual = sut.translate(context, "");

    EXPECT_EQ(std::string{}, actual);
}

TEST_F(i18n_identity_provider_test_t, empty_context)
{
    auto const actual = sut.translate("", source);

    EXPECT_EQ(source, actual);
}

TEST_F(i18n_identity_provider_test_t, both_empty)
{
    auto const actual = sut.translate("", "");

    EXPECT_EQ(std::string{}, actual);
}

TEST_F(i18n_identity_provider_test_t, null_source)
{
    auto const actual = sut.translate(context, nullptr);

    EXPECT_EQ(std::string{}, actual);
}

TEST_F(i18n_identity_provider_test_t, null_context)
{
    auto const actual = sut.translate(nullptr, source);

    EXPECT_EQ(source, actual);
}

TEST_F(i18n_identity_provider_test_t, both_null)
{
    auto const actual = sut.translate(nullptr, nullptr);

    EXPECT_EQ(std::string{}, actual);
}

//
// global provider instance
//

// tests modifying the global provider
struct i18n_global_provider_test_t : i18n_test_t
{
    struct provider_t : provider_i
    {
        virtual ~provider_t() = default;
        auto translate(char const*, char const*) const -> std::string override { return std::string{}; }
    };
    provider_t new_provider;
};

TEST_F(i18n_global_provider_test_t, default_is_identity)
{
    auto const actual = dynamic_cast<identity_provider_t const*>(&provider());
    EXPECT_NE(nullptr, actual);
}

TEST_F(i18n_global_provider_test_t, assigning_returns_previous)
{
    auto& expected = provider();

    auto& actual = provider(new_provider);

    EXPECT_EQ(&expected, &actual);
}

TEST_F(i18n_global_provider_test_t, state_saver_saves_and_restores)
{
    auto& initial = provider();

    {
        auto scoped_provider = scoped_provider_t{new_provider};

        EXPECT_EQ(&new_provider, &provider());
    }

    EXPECT_EQ(&initial, &provider());
}

// tests using the global provider
struct i18n_global_provider_mock_test_t : i18n_test_t
{
    struct mock_provider_t : provider_i
    {
        virtual ~mock_provider_t() = default;

        MOCK_METHOD(std::string, translate, (char const* context, char const* source), (const, override));
    };
    StrictMock<mock_provider_t> mock_provider;

    scoped_provider_t scoped_provider{mock_provider};

    std::string expected = "expected";
};

TEST_F(i18n_global_provider_mock_test_t, crv_tr_forwards_correctly)
{
    EXPECT_CALL(mock_provider, translate("", source)).WillOnce(Return(expected));

    auto const actual = CRV_TR(source);

    EXPECT_EQ(expected, actual);
}

TEST_F(i18n_global_provider_mock_test_t, crv_tr_c_forwards_correctly)
{
    EXPECT_CALL(mock_provider, translate(context, source)).WillOnce(Return(expected));

    auto const actual = CRV_TR_C(context, source);

    EXPECT_EQ(expected, actual);
}

TEST_F(i18n_global_provider_mock_test_t, crv_tr_formats_arguments)
{
    EXPECT_CALL(mock_provider, translate("", source)).WillOnce(Return("{} unordered {}"));

    auto const actual = CRV_TR(source, 2, "args");

    EXPECT_EQ("2 unordered args", actual);
}

TEST_F(i18n_global_provider_mock_test_t, crv_tr_c_formats_indexed_arguments)
{
    EXPECT_CALL(mock_provider, translate(context, source)).WillOnce(Return("{1} ordered before {0}"));

    auto const actual = CRV_TR_C(context, source, "second", 1);

    EXPECT_EQ("1 ordered before second", actual);
}

TEST_F(i18n_global_provider_mock_test_t, formats_correctly_with_escaped_braces)
{
    EXPECT_CALL(mock_provider, translate(context, source)).WillOnce(Return("value {{{0}}}"));

    auto const actual = CRV_TR_C(context, source, 37);

    EXPECT_EQ("value {37}", actual);
}

} // namespace
} // namespace crv::i18n
