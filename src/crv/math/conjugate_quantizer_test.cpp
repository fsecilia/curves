// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "conjugate_quantizer.hpp"
#include <crv/test/test.hpp>
#include <gmock/gmock.h>

namespace crv {
namespace {

struct conjugate_quantizer_test_t : Test
{
    using real_t = float_t;

    static constexpr auto quantum = 0.25;
    static constexpr auto unprojected_min = 2.0;
    static constexpr auto unprojected_max = 15.9;
    static constexpr auto projected = 4.0;
    static constexpr auto unprojected = 7.874;

    struct mock_projection_t
    {
        virtual ~mock_projection_t() = default;
        MOCK_METHOD(real_t, call, (real_t), (const, noexcept));
    };
    StrictMock<mock_projection_t> mock_projection;

    struct projection_t
    {
        mock_projection_t* mock = nullptr;
        auto operator()(real_t projected) const noexcept -> real_t { return mock->call(projected); }
        auto operator==(projection_t const&) const noexcept -> bool = default;
    };
    projection_t projection{&mock_projection};

    struct mock_inverter_t
    {
        virtual ~mock_inverter_t() = default;
        MOCK_METHOD(std::optional<real_t>, call, (real_t, real_t, real_t, projection_t), (const, noexcept));
    };
    StrictMock<mock_inverter_t> mock_inverter;

    struct inverter_t
    {
        mock_inverter_t* mock = nullptr;
        auto operator()(real_t unprojected_min, real_t unprojected_max, real_t projected,
            projection_t projection) const noexcept -> std::optional<real_t>
        {
            return mock->call(unprojected_min, unprojected_max, projected, std::move(projection));
        }
        auto operator==(inverter_t const&) const noexcept -> bool = default;
    };

    using sut_t = conjugate_quantizer_t<real_t, quantum, projection_t, inverter_t>;
    sut_t sut{unprojected_min, unprojected_max, projection, inverter_t{&mock_inverter}};
};

TEST_F(conjugate_quantizer_test_t, inversion_successful)
{
    EXPECT_CALL(mock_inverter, call(unprojected_min, unprojected_max, projected, projection))
        .WillOnce(Return(std::optional{unprojected}));

    // round(7.874/0.25)*0.25 = round(31.496)*0.25 = 31.0*0.25 = 7.75
    auto const expected_quantized_unprojected = 7.75;
    auto const expected = 32.0;
    EXPECT_CALL(mock_projection, call(expected_quantized_unprojected)).WillOnce(Return(expected));

    auto const actual = sut(projected);

    EXPECT_EQ(expected, actual);
}

TEST_F(conjugate_quantizer_test_t, inversion_unsuccessful)
{
    EXPECT_CALL(mock_inverter, call(unprojected_min, unprojected_max, projected, projection))
        .WillOnce(Return(std::nullopt));

    // round(15.9/0.25)*0.25 = round(63.6)*0.25 = 64.0*0.25 = 16.0
    auto const expected_quantized_unprojected = real_t{16.0};
    auto const expected = real_t{32.0};
    EXPECT_CALL(mock_projection, call(expected_quantized_unprojected)).WillOnce(Return(expected));

    auto const actual = sut(projected);

    EXPECT_EQ(expected, actual);
}

} // namespace
} // namespace crv
