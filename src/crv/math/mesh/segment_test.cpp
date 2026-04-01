// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "segment.hpp"
#include <crv/test/test.hpp>
#include <gmock/gmock.h>

namespace crv {
namespace {

struct mesh_segment_pred_test_t : Test
{
    using real_t           = float_t;
    using measured_error_t = measured_error_t<real_t>;
    using payload_t        = int_t;

    struct mock_measured_error_pred_t
    {
        MOCK_METHOD(bool, call, (measured_error_t const& left, measured_error_t const& right), (const, noexcept));

        virtual ~mock_measured_error_pred_t() = default;
    };
    StrictMock<mock_measured_error_pred_t> mock_measured_error_pred;

    struct measured_error_pred_t
    {
        mock_measured_error_pred_t* mock = nullptr;

        auto operator()(auto const& left, auto const& right) const noexcept -> bool { return mock->call(left, right); }
    };

    using mesh_segment_t = mesh_segment_t<real_t, payload_t>;
    mesh_segment_t const left{};
    mesh_segment_t const right{};

    using sut_t = mesh_segment_pred_t<measured_error_pred_t>;
    sut_t sut{&mock_measured_error_pred};
};

TEST_F(mesh_segment_pred_test_t, call_delegates_to_measured_error_pred_true)
{
    auto const expected = true;
    EXPECT_CALL(mock_measured_error_pred, call(Ref(left.max_measured_error), Ref(right.max_measured_error)))
        .WillOnce(Return(expected));

    auto const actual = sut(left, right);

    EXPECT_EQ(expected, actual);
}

TEST_F(mesh_segment_pred_test_t, call_delegates_to_measured_error_pred_false)
{
    auto const expected = false;
    EXPECT_CALL(mock_measured_error_pred, call(Ref(left.max_measured_error), Ref(right.max_measured_error)))
        .WillOnce(Return(expected));

    auto const actual = sut(left, right);

    EXPECT_EQ(expected, actual);
}

} // namespace
} // namespace crv
