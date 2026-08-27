// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "session_model.hpp"
#include <crv/test/test.hpp>
#include <expected>
#include <gmock/gmock.h>
#include <vector>

namespace crv::qt {
namespace {

using namespace testing;

struct session_model_test_t : Test
{
    using devices_result_t = std::expected<std::vector<pipeline::control::attachment_t>, pipeline::control::error_t>;
    using apply_error_t = std::variant<uint8_t, pipeline::control::error_t>;
    using apply_result_t = std::expected<void, apply_error_t>;

    struct mock_session_t
    {
        MOCK_METHOD(devices_result_t, devices, (), (const));
        MOCK_METHOD(apply_result_t, apply,
            (model::device_t const&, model::profile_t const&, pipeline::control::attachment_id_t,
                pipeline::configuration::apply_mode_t),
            (const));
    };

    struct session_t
    {
        using devices_result_t = session_model_test_t::devices_result_t;
        using apply_result_t = session_model_test_t::apply_result_t;
        mock_session_t* mock;

        auto devices() const -> devices_result_t { return mock->devices(); }
        auto apply(model::device_t const& device, model::profile_t const& profile,
            pipeline::control::attachment_id_t attachment, pipeline::configuration::apply_mode_t mode) const
            -> apply_result_t
        {
            return mock->apply(device, profile, attachment, mode);
        }
    };

    using sut_t = generic::session_model_t<session_t>;

    StrictMock<mock_session_t> mock_session;
    sut_t sut{{&mock_session}};
    model::device_t device;
    model::profile_t profile;

    static auto attachment(uint64_t id, std::string sysname) -> pipeline::control::attachment_t
    {
        return {.id = pipeline::control::attachment_id_t{id}, .sysname = std::move(sysname)};
    }
};

TEST_F(session_model_test_t, initial_single_attachment_is_selected)
{
    EXPECT_CALL(mock_session, devices).WillOnce(Return(std::vector{attachment(17, "input17")}));

    (void)sut.refresh();

    EXPECT_EQ(sut.selected_attachment_id(), pipeline::control::attachment_id_t{17});
}

TEST_F(session_model_test_t, initial_multiple_attachments_have_no_selection)
{
    EXPECT_CALL(mock_session, devices)
        .WillOnce(Return(std::vector{attachment(17, "input17"), attachment(23, "input23")}));

    (void)sut.refresh();

    EXPECT_FALSE(sut.has_selected_attachment());
}

TEST_F(session_model_test_t, initial_empty_attachment_list_has_no_selection)
{
    EXPECT_CALL(mock_session, devices).WillOnce(Return(std::vector<pipeline::control::attachment_t>{}));

    (void)sut.refresh();

    EXPECT_FALSE(sut.has_selected_attachment());
}

TEST_F(session_model_test_t, refresh_preserves_selected_attachment_by_id)
{
    EXPECT_CALL(mock_session, devices)
        .WillOnce(Return(std::vector{attachment(11, "input11"), attachment(29, "input29")}))
        .WillOnce(Return(std::vector{attachment(29, "input29"), attachment(11, "input11")}));
    (void)sut.refresh();
    sut.set_selected_attachment_index(1);

    (void)sut.refresh();

    EXPECT_EQ(sut.selected_attachment_id(), pipeline::control::attachment_id_t{29});
}

TEST_F(session_model_test_t, later_single_attachment_does_not_autoselect_after_initial_multiple)
{
    EXPECT_CALL(mock_session, devices)
        .WillOnce(Return(std::vector{attachment(11, "input11"), attachment(29, "input29")}))
        .WillOnce(Return(std::vector{attachment(41, "input41")}));
    (void)sut.refresh();

    (void)sut.refresh();

    EXPECT_FALSE(sut.has_selected_attachment());
}

TEST_F(session_model_test_t, refresh_clears_vanished_selection_without_selecting_replacement)
{
    EXPECT_CALL(mock_session, devices)
        .WillOnce(Return(std::vector{attachment(11, "input11"), attachment(29, "input29")}))
        .WillOnce(Return(std::vector{attachment(41, "input41")}));
    (void)sut.refresh();
    sut.set_selected_attachment_index(0);

    (void)sut.refresh();

    EXPECT_FALSE(sut.has_selected_attachment());
}

TEST_F(session_model_test_t, enumeration_failure_preserves_previous_snapshot_and_selection)
{
    auto const failure
        = pipeline::control::error_t{.code = pipeline::control::error_code_t::enumeration_failed, .native_error = EIO};
    EXPECT_CALL(mock_session, devices)
        .WillOnce(Return(std::vector{attachment(11, "input11")}))
        .WillOnce(Return(std::unexpected{failure}));
    (void)sut.refresh();

    (void)sut.refresh();

    EXPECT_EQ(sut.selected_attachment_id(), pipeline::control::attachment_id_t{11});
}

TEST_F(session_model_test_t, apply_resolves_selected_row_to_exact_attachment_and_active_mode)
{
    EXPECT_CALL(mock_session, devices)
        .WillOnce(Return(std::vector{attachment(7, "input7"), attachment(0x20000000000001, "input9")}));
    (void)sut.refresh();
    sut.set_selected_attachment_index(1);
    EXPECT_CALL(mock_session,
        apply(Ref(device), Ref(profile), pipeline::control::attachment_id_t{0x20000000000001},
            pipeline::configuration::apply_mode_t::active))
        .WillOnce(Return(apply_result_t{}));

    auto const result = sut.apply(device, profile, pipeline::configuration::apply_mode_t::active);

    EXPECT_TRUE(result.has_value());
}

TEST_F(session_model_test_t, apply_forwards_bypassed_mode)
{
    EXPECT_CALL(mock_session, devices).WillOnce(Return(std::vector{attachment(7, "input7")}));
    (void)sut.refresh();
    EXPECT_CALL(mock_session,
        apply(Ref(device), Ref(profile), pipeline::control::attachment_id_t{7},
            pipeline::configuration::apply_mode_t::bypassed))
        .WillOnce(Return(apply_result_t{}));

    auto const result = sut.apply(device, profile, pipeline::configuration::apply_mode_t::bypassed);

    EXPECT_TRUE(result.has_value());
}

} // namespace
} // namespace crv::qt
