// SPDX-License-Identifier: MIT

/// \file
/// \brief compiled userspace runtime configuration
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/pipeline.hpp>
#include <cstddef>
#include <type_traits>

namespace crv::pipeline::configuration {

struct runtime_t
{
    pipeline_t::config_t config{};
    pipeline_t::gain_t gain{};
};

static_assert(std::is_trivially_copyable_v<runtime_t>);
static_assert(alignof(runtime_t) == 64);
static_assert(offsetof(runtime_t, config) == 0);
static_assert(offsetof(runtime_t, gain) == 64);

} // namespace crv::pipeline::configuration
