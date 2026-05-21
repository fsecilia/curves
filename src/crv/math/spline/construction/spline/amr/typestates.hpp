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

    struct partitioned_t
    {
        workspace_t& workspace;
        using next_t = seeded_t;

        explicit partitioned_t(workspace_t& w) : workspace{w} {}
        partitioned_t(partitioned_t const&) = delete;
        partitioned_t& operator=(partitioned_t const&) = delete;
        partitioned_t(partitioned_t&&) = default;
        partitioned_t& operator=(partitioned_t&&) = default;
    };

    struct uninitialized_t
    {
        workspace_t& workspace;
        using next_t = partitioned_t;

        explicit uninitialized_t(workspace_t& w) : workspace{w} {}
        uninitialized_t(uninitialized_t const&) = delete;
        uninitialized_t& operator=(uninitialized_t const&) = delete;
        uninitialized_t(uninitialized_t&&) = default;
        uninitialized_t& operator=(uninitialized_t&&) = default;
    };

    using initial_t = uninitialized_t;
};

} // namespace crv::spline
