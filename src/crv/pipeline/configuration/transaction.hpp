// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <expected>
#include <utility>

namespace crv::pipeline::configuration {

/// coordinates candidate validation and commit dependencies
template <typename validator_t, typename committer_t> struct transaction_t
{
    [[no_unique_address]] validator_t validator{};
    [[no_unique_address]] committer_t committer{};

    template <typename candidate_t> struct validated_candidate_t
    {
        candidate_t const& candidate;
    };

    template <typename candidate_t>
    using validation_result_t = decltype(std::declval<validator_t const&>()(
        std::declval<candidate_t const&>().config, std::declval<candidate_t const&>().gain));

    template <typename candidate_t>
    constexpr auto validate(candidate_t const& candidate) const noexcept
        -> std::expected<validated_candidate_t<candidate_t>, validation_result_t<candidate_t>>
    {
        auto result = validator(candidate.config, candidate.gain);
        if (!result) return std::unexpected{std::move(result)};

        return validated_candidate_t<candidate_t>{candidate};
    }

    template <typename target_t, typename candidate_t>
    constexpr auto commit(target_t& target, validated_candidate_t<candidate_t> const& validated) const noexcept
        -> void
    {
        committer(target, validated);
    }
};

} // namespace crv::pipeline::configuration
