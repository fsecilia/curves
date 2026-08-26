// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "config_builder.hpp"
#include <crv/test/test.hpp>
#include <array>
#include <gmock/gmock.h>

namespace crv::pipeline::configuration {
namespace {

using velocity_scale_t = pipeline_t::velocity_scale_t;
using duration_t = pipeline_t::duration_t;
using output_transform_t = decltype(pipeline_t::config_t{}.output_transform);

static_assert(velocity_scale_builder_t{}(1000) == velocity_scale_t{1'000'000});
static_assert(velocity_scale_builder_t{}(1) == velocity_scale_t{1'000'000'000});
static_assert(velocity_scale_builder_t{}(int_t{1} << 44) == velocity_scale_t::literal(976'562));

struct half_life_vector_t
{
    float_t milliseconds;
    uint64_t nanoseconds;
};

struct half_life_builder_test_t : TestWithParam<half_life_vector_t>
{};

TEST_P(half_life_builder_test_t, rounds_to_nearest_even_nanosecond)
{
    auto const& [milliseconds, nanoseconds] = GetParam();
    EXPECT_EQ(half_life_builder_t{}(milliseconds), duration_t{nanoseconds});
}

constexpr auto half_life_vectors = std::array{
    half_life_vector_t{0.0, 0},
    half_life_vector_t{2.0, 2'000'000},
    half_life_vector_t{1000.0, 1'000'000'000},
    half_life_vector_t{0.0000015, 2},
    half_life_vector_t{0.0000025, 2},
};
INSTANTIATE_TEST_SUITE_P(conversion, half_life_builder_test_t, ValuesIn(half_life_vectors));

struct config_builder_test_t : Test
{
    struct mock_t
    {
        MOCK_METHOD(velocity_scale_t, velocity_scale, (int_t), (const));
        MOCK_METHOD(duration_t, half_life, (float_t), (const));
        MOCK_METHOD(output_transform_t, output_transform, (float_t, float_t), (const));
    };
    StrictMock<mock_t> mock;

    struct velocity_scale_delegate_t
    {
        mock_t* mock;
        auto operator()(int_t dpi) const -> velocity_scale_t { return mock->velocity_scale(dpi); }
    };

    struct half_life_delegate_t
    {
        mock_t* mock;
        auto operator()(float_t milliseconds) const -> duration_t { return mock->half_life(milliseconds); }
    };

    struct output_transform_delegate_t
    {
        mock_t* mock;
        auto operator()(float_t rotation, float_t anisotropy) const -> output_transform_t
        {
            return mock->output_transform(rotation, anisotropy);
        }
    };

    using sut_t = config_builder_t<velocity_scale_delegate_t, half_life_delegate_t, output_transform_delegate_t>;

    model::device_t device;
    model::profile_t profile;
    sut_t sut{{&mock}, {&mock}, {&mock}};

    config_builder_test_t()
    {
        device.dpi.value(800);
        device.rotation.value(45.0);
        profile.filter_halflife.value(3.0);
        profile.anisotropy.value(2.0);
    }
};

TEST_F(config_builder_test_t, composes_scalar_runtime_configuration)
{
    auto const velocity_scale = velocity_scale_t{17};
    auto const half_life = duration_t{23};
    auto const output_transform = output_transform_t{};
    EXPECT_CALL(mock, velocity_scale(800)).WillOnce(Return(velocity_scale));
    EXPECT_CALL(mock, half_life(3.0)).WillOnce(Return(half_life));
    EXPECT_CALL(mock, output_transform(45.0, 2.0)).WillOnce(Return(output_transform));

    auto const result = sut(device, profile);

    EXPECT_TRUE(result.velocity_scale == velocity_scale && result.half_life == half_life
        && result.output_transform.matrix == output_transform.matrix);
}

} // namespace
} // namespace crv::pipeline::configuration
