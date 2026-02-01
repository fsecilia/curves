// SPDX-License-Identifier: MIT
/**
    \file
    \brief tests jet composition over jets

    These tests verify that autodiff composes correctly to calc second derivatives via jet_t<jet_t<double>>.

    \copyright Copyright (C) 2026 Frank Secilia
*/

#include <crv/math/jet/jet.hpp>
#include <crv/test/test.hpp>

namespace crv {
namespace {

struct nested_jet_test_t : Test
{
    using scalar_t = double;
    using value_t  = jet_t<scalar_t>;
    using sut_t    = jet_t<value_t>;

    // seed with primish numbers
    static constexpr scalar_t s{1.3};
    static constexpr value_t  v{1.7, 1.9};
    static constexpr sut_t    x{{2.3, 3.1}, {5.3, 7.1}};
    static constexpr sut_t    y{{5.9, 7.3}, {8.3, 9.7}};

    static constexpr scalar_t eps = 1e-10;
};

} // namespace
} // namespace crv
