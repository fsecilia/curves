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

    struct unassembled_t
    {
        workspace_t& workspace;

        explicit unassembled_t(workspace_t& w) : workspace{w} {}
        unassembled_t(unassembled_t const&) = delete;
        unassembled_t& operator=(unassembled_t const&) = delete;
        unassembled_t(unassembled_t&&) = default;
        unassembled_t& operator=(unassembled_t&&) = default;
    };

    struct unrefined_t
    {
        workspace_t& workspace;
        using next_t = unassembled_t;

        explicit unrefined_t(workspace_t& w) : workspace{w} {}
        unrefined_t(unrefined_t const&) = delete;
        unrefined_t& operator=(unrefined_t const&) = delete;
        unrefined_t(unrefined_t&&) = default;
        unrefined_t& operator=(unrefined_t&&) = default;
    };

    struct unseeded_t
    {
        workspace_t& workspace;
        using next_t = unrefined_t;

        explicit unseeded_t(workspace_t& w) : workspace{w} {}
        unseeded_t(unseeded_t const&) = delete;
        unseeded_t& operator=(unseeded_t const&) = delete;
        unseeded_t(unseeded_t&&) = default;
        unseeded_t& operator=(unseeded_t&&) = default;
    };

    struct uninitialized_t
    {
        workspace_t& workspace;
        using next_t = unseeded_t;

        explicit uninitialized_t(workspace_t& w) : workspace{w} {}
        uninitialized_t(uninitialized_t const&) = delete;
        uninitialized_t& operator=(uninitialized_t const&) = delete;
        uninitialized_t(uninitialized_t&&) = default;
        uninitialized_t& operator=(uninitialized_t&&) = default;
    };

    using initial_t = uninitialized_t;
};

} // namespace crv::spline
