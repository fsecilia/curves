// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/pipeline.hpp>
#include <crv/pipeline/configuration/apply_mode.hpp>
#include <cstddef>

namespace crv::pipeline::configuration {

struct candidate_t
{
    crv::pipeline_t::config_t config{};
    apply_mode_t mode = apply_mode_t::active;
    crv::pipeline_t::gain_t gain{};
};

static_assert(sizeof(candidate_t) == 11'136);
static_assert(alignof(candidate_t) == 64);
static_assert(offsetof(candidate_t, config) == 0);
static_assert(offsetof(candidate_t, mode) == 56);
static_assert(offsetof(candidate_t, gain) == 64);

} // namespace crv::pipeline::configuration
