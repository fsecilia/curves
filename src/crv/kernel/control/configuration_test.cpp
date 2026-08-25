// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "configuration.h"
#include "configuration.hpp"
#include <crv/kernel/control/abi.h>
#include <crv/pipeline/configuration/transaction.hpp>
#include <crv/test/test.hpp>
#include <cstddef>
#include <cstring>
#include <gmock/gmock.h>
#include <vector>

namespace crv::kernel::control {
namespace {

static_assert(apply_mode_decoder_t{}(CRV_CONTROL_APPLY_MODE_BYPASSED)
    == pipeline::configuration::apply_mode_t::bypassed);
static_assert(apply_mode_decoder_t{}(CRV_CONTROL_APPLY_MODE_ACTIVE) == pipeline::configuration::apply_mode_t::active);
static_assert(!apply_mode_decoder_t{}(2));

struct configuration_test_t : Test
{
    struct config_t
    {
        uint64_t value{};
    };

    struct gain_t
    {
        uint64_t value{};
    };

    struct candidate_t
    {
        config_t config{};
        pipeline::configuration::apply_mode_t mode = pipeline::configuration::apply_mode_t::active;
        gain_t gain{};
    };

    struct validation_result_t
    {
        int_t error{};

        constexpr explicit operator bool() const noexcept { return error == 0; }
        constexpr auto operator==(validation_result_t const&) const noexcept -> bool = default;
    };

    struct target_t
    {
        int_t commits{};
    };

    struct mock_validator_t
    {
        virtual ~mock_validator_t() = default;
        MOCK_METHOD(validation_result_t, call, (config_t const&, gain_t const&), (const, noexcept));
    };
    StrictMock<mock_validator_t> mock_validator;

    struct validator_t
    {
        using result_t = validation_result_t;

        mock_validator_t* mock = nullptr;

        auto operator()(config_t const& config, gain_t const& gain) const noexcept -> validation_result_t
        {
            return mock->call(config, gain);
        }
    };

    struct mock_committer_t
    {
        virtual ~mock_committer_t() = default;
        MOCK_METHOD(void, call, (target_t&, candidate_t const&), (const, noexcept));
    };
    StrictMock<mock_committer_t> mock_committer;

    struct committer_t
    {
        mock_committer_t* mock = nullptr;

        template <typename validated_candidate_t>
        auto operator()(target_t& target, validated_candidate_t const& validated) const noexcept -> void
        {
            mock->call(target, validated.candidate);
        }
    };

    using transaction_t = pipeline::configuration::transaction_t<validator_t, committer_t>;
    using sut_t = configuration_t<candidate_t, transaction_t>;

    sut_t sut{pipeline::configuration::apply_mode_t::active,
        {.validator = {&mock_validator}, .committer = {&mock_committer}}};
};

TEST_F(configuration_test_t, config_bytes_are_candidate_config_storage)
{
    auto bytes = sut.config_bytes();
    auto const expected = uint64_t{0x2a};
    std::memcpy(bytes.data(), &expected, sizeof(expected));

    EXPECT_CALL(mock_validator, call(Field(&config_t::value, expected), testing::_))
        .WillOnce(Return(validation_result_t{}));
    (void)sut.validate();
}

TEST_F(configuration_test_t, gain_bytes_are_candidate_gain_storage)
{
    auto bytes = sut.gain_bytes();
    auto const expected = uint64_t{0x2a};
    std::memcpy(bytes.data(), &expected, sizeof(expected));

    EXPECT_CALL(mock_validator, call(testing::_, Field(&gain_t::value, expected)))
        .WillOnce(Return(validation_result_t{}));
    (void)sut.validate();
}

TEST_F(configuration_test_t, failed_validation_keeps_configuration_unvalidated)
{
    EXPECT_CALL(mock_validator, call).WillOnce(Return(validation_result_t{.error = 7}));

    (void)sut.validate();

    EXPECT_FALSE(sut.validated());
}

TEST_F(configuration_test_t, successful_validation_enables_commit)
{
    EXPECT_CALL(mock_validator, call).WillOnce(Return(validation_result_t{}));
    (void)sut.validate();

    target_t target{};
    EXPECT_CALL(mock_committer, call(Ref(target), testing::_));

    sut.commit(target);
}

struct kernel_control_configuration_bridge_test_t : Test
{
    std::size_t const storage_size = crv_control_prepared_configuration_storage_size();
    std::vector<std::byte> storage = std::vector<std::byte>(storage_size + 1);
};

TEST_F(kernel_control_configuration_bridge_test_t, reports_uapi_config_size)
{
    EXPECT_EQ(sizeof(crv_control_runtime_config_v1_t), crv_control_prepared_configuration_config_size());
}

TEST_F(kernel_control_configuration_bridge_test_t, reports_uapi_gain_size)
{
    EXPECT_EQ(sizeof(crv_control_gain_v1_t), crv_control_prepared_configuration_gain_size());
}

TEST_F(kernel_control_configuration_bridge_test_t, aligns_prepared_configuration_in_c_owned_storage)
{
    auto* unaligned_storage = static_cast<void*>(storage.data());
    if (reinterpret_cast<uint_t>(unaligned_storage) % 64U == 0) unaligned_storage = storage.data() + 1;

    auto* const aligned_candidate
        = crv_control_prepared_configuration_construct(unaligned_storage, CRV_CONTROL_APPLY_MODE_ACTIVE);

    EXPECT_EQ(0U, reinterpret_cast<uint_t>(aligned_candidate) % 64U);

    crv_control_prepared_configuration_destroy(aligned_candidate);
}

TEST_F(kernel_control_configuration_bridge_test_t, rejects_unknown_external_apply_mode)
{
    EXPECT_EQ(nullptr, crv_control_prepared_configuration_construct(storage.data(), 2));
}

} // namespace
} // namespace crv::kernel::control
