// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/pipeline/configuration/apply_mode.hpp>
#include <cassert>
#include <type_traits>

namespace crv::pipeline::configuration {

/// commits a validated configuration candidate into live pipeline storage
struct committer_t
{
    template <typename target_t, typename validated_candidate_t>
    constexpr auto operator()(target_t& target, validated_candidate_t const& validated) const noexcept -> void
    {
        auto const& candidate = validated.candidate;
        target.commit_configuration([&](auto& config, auto& gain, auto& state, auto& mode) noexcept
        {
            using mode_t = std::remove_cvref_t<decltype(mode)>;
            auto const target_mode = map_mode<mode_t>(candidate.mode);

            config = candidate.config;
            gain = candidate.gain;
            state = {};
            mode = target_mode;
        });
    }

private:
    template <typename mode_t>
    static constexpr auto map_mode(apply_mode_t mode) noexcept -> mode_t
    {
        switch (mode)
        {
            case apply_mode_t::bypassed: return mode_t::bypassed;
            case apply_mode_t::active: return mode_t::active;
        }

        assert(false && "configuration apply mode out of range");
        __builtin_unreachable();
    }
};

} // namespace crv::pipeline::configuration
