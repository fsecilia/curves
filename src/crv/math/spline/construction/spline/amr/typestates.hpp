// SPDX-License-Identifier: MIT

/// \file
/// \brief compile-time state encoding
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>

namespace crv::spline {

/// encodes workspace runtime state in its compile-time type
template <typename t_workspace_t> struct typestates_t
{
    using workspace_t = t_workspace_t;

    struct refined_t
    {
        workspace_t& workspace;

        explicit refined_t(workspace_t& w) : workspace{w} {}
        refined_t(refined_t const&) = delete;
        refined_t& operator=(refined_t const&) = delete;
        refined_t(refined_t&&) = default;
        refined_t& operator=(refined_t&&) = default;
    };

    struct seeded_t
    {
        workspace_t& workspace;
        using next_t = refined_t;

        explicit seeded_t(workspace_t& w) : workspace{w} {}
        seeded_t(seeded_t const&) = delete;
        seeded_t& operator=(seeded_t const&) = delete;
        seeded_t(seeded_t&&) = default;
        seeded_t& operator=(seeded_t&&) = default;
    };

    struct unseeded_t
    {
        workspace_t& workspace;
        using next_t = seeded_t;

        explicit unseeded_t(workspace_t& w) : workspace{w} {}
        unseeded_t(unseeded_t const&) = delete;
        unseeded_t& operator=(unseeded_t const&) = delete;
        unseeded_t(unseeded_t&&) = default;
        unseeded_t& operator=(unseeded_t&&) = default;
    };
};

} // namespace crv::spline
