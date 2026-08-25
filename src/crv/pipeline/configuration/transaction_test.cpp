// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "transaction.hpp"
#include "apply_mode.hpp"
#include <crv/test/test.hpp>
#include <gmock/gmock.h>

namespace crv::pipeline::configuration {
namespace {

struct transaction_test_t : Test
{
    struct config_t
    {
        int_t value{};
    };

    struct gain_t
    {
        int_t value{};
    };

    struct candidate_t
    {
        config_t config{};
        gain_t gain{};
        apply_mode_t mode = apply_mode_t::active;
    };

    struct validation_result_t
    {
        int_t error{};

        constexpr explicit operator bool() const noexcept { return error == 0; }
        constexpr auto operator==(validation_result_t const&) const noexcept -> bool = default;
    };

    struct mock_validator_t
    {
        virtual ~mock_validator_t() = default;
        MOCK_METHOD(validation_result_t, call, (config_t const&, gain_t const&), (const, noexcept));
    };
    StrictMock<mock_validator_t> mock_validator;

    struct validator_t
    {
        mock_validator_t* mock = nullptr;

        auto operator()(config_t const& config, gain_t const& gain) const noexcept -> validation_result_t
        {
            return mock->call(config, gain);
        }
    };

    struct target_t
    {
        int_t value{};
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

    using sut_t = transaction_t<validator_t, committer_t>;

    candidate_t candidate{.config = {.value = 11}, .gain = {.value = 22}, .mode = apply_mode_t::bypassed};
    target_t target{.value = 33};
    sut_t sut{.validator = {&mock_validator}, .committer = {&mock_committer}};
};

TEST_F(transaction_test_t, validate_delegates_candidate_data_to_validator)
{
    EXPECT_CALL(mock_validator, call(Ref(candidate.config), Ref(candidate.gain)))
        .WillOnce(Return(validation_result_t{}));

    (void)sut.validate(candidate);
}

TEST_F(transaction_test_t, successful_validation_borrows_candidate)
{
    EXPECT_CALL(mock_validator, call).WillOnce(Return(validation_result_t{}));

    auto const result = sut.validate(candidate);

    EXPECT_EQ(result ? &result->candidate : nullptr, &candidate);
}

TEST_F(transaction_test_t, failed_validation_returns_validator_result)
{
    auto const expected = validation_result_t{.error = 7};
    EXPECT_CALL(mock_validator, call).WillOnce(Return(expected));

    EXPECT_EQ(sut.validate(candidate), std::unexpected{expected});
}

TEST_F(transaction_test_t, commit_delegates_validated_candidate_to_committer)
{
    auto const validated = sut_t::validated_candidate_t<candidate_t>{candidate};
    EXPECT_CALL(mock_committer, call(Ref(target), Ref(candidate)));

    sut.commit(target, validated);
}

} // namespace
} // namespace crv::pipeline::configuration
