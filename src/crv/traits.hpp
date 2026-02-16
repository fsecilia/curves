// SPDX-License-Identifier: MIT
/*!
    \file
    \brief common type traits

    \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <crv/lib.hpp>

namespace crv {

// --------------------------------------------------------------------------------------------------------------------
// copy_cv_t
// --------------------------------------------------------------------------------------------------------------------

namespace detail {

template <typename dst_t, typename src_t> struct copy_cv_f;

template <typename dst_t, typename src_t> struct copy_cv_f
{
    using type = dst_t;
};

template <typename dst_t, typename src_t> struct copy_cv_f<dst_t, src_t const>
{
    using type = dst_t const;
};

template <typename dst_t, typename src_t> struct copy_cv_f<dst_t, src_t volatile>
{
    using type = dst_t volatile;
};

template <typename dst_t, typename src_t> struct copy_cv_f<dst_t, src_t const volatile>
{
    using type = dst_t const volatile;
};

} // namespace detail

/// applies same const and volatile qualifiers as applied to src_t to dst_t
template <typename dst_t, typename src_t> using copy_cv_t = detail::copy_cv_f<dst_t, src_t>::type;

} // namespace crv
