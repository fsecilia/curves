// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "smootheststep.hpp"
#include <crv/model/shaping/transitions/smootherstep.hpp>
#include <crv/model/shaping/transitions/smoothstep.hpp>
#include <crv/test/test.hpp>

namespace crv::shaping::transitions {
namespace {

template <typename transition_t, typename scalar_t, scalar_t midpoint_derivative, scalar_t quarter_antiderivative,
    scalar_t midpoint_antiderivative>
struct polynomial_contract_test_t
{
    static constexpr auto sut = transition_t{};
    using jet_t = crv::jet_t<scalar_t>;

    static_assert(sut(scalar_t{-1}) == scalar_t{0});
    static_assert(sut(scalar_t{0}) == scalar_t{0});
    static_assert(sut(scalar_t{0.5}) == scalar_t{0.5});
    static_assert(sut(scalar_t{1}) == scalar_t{1});
    static_assert(sut(scalar_t{2}) == scalar_t{1});

    static_assert(sut.derivative(scalar_t{-1}) == scalar_t{0});
    static_assert(sut.derivative(scalar_t{0}) == scalar_t{0});
    static_assert(sut.derivative(scalar_t{0.5}) == midpoint_derivative);
    static_assert(sut.derivative(scalar_t{1}) == scalar_t{0});
    static_assert(sut.derivative(scalar_t{2}) == scalar_t{0});

    static_assert(sut.antiderivative(scalar_t{-1}) == scalar_t{0});
    static_assert(sut.antiderivative(scalar_t{0}) == scalar_t{0});
    static_assert(sut.antiderivative(scalar_t{0.25}) == quarter_antiderivative);
    static_assert(sut.antiderivative(scalar_t{0.5}) == midpoint_antiderivative);
    static_assert(sut.antiderivative(scalar_t{1}) == scalar_t{0.5});
    static_assert(sut.antiderivative(scalar_t{2}) == scalar_t{1.5});

    static_assert(sut(jet_t{scalar_t{0.5}, scalar_t{2}}) == jet_t{scalar_t{0.5}, scalar_t{2} * midpoint_derivative});
    static_assert(sut.antiderivative(jet_t{scalar_t{0.5}, scalar_t{2}}) == jet_t{midpoint_antiderivative, scalar_t{1}});
    static_assert(sut.antiderivative(jet_t{scalar_t{-1}, scalar_t{3}}) == jet_t{scalar_t{0}, scalar_t{0}});
    static_assert(sut.antiderivative(jet_t{scalar_t{2}, scalar_t{3}}) == jet_t{scalar_t{1.5}, scalar_t{3}});

    static_assert(sut(scalar_t{0.25}) == scalar_t{1} - sut(scalar_t{0.75}));
    static_assert(sut.derivative(scalar_t{0.25}) == sut.derivative(scalar_t{0.75}));
};

using smoothstep_float_test_t = polynomial_contract_test_t<smoothstep_t, float32_t, 1.5F, 0.013671875F, 0.09375F>;
using smoothstep_double_test_t = polynomial_contract_test_t<smoothstep_t, float64_t, 1.5, 0.013671875, 0.09375>;
using smootherstep_float_test_t
    = polynomial_contract_test_t<smootherstep_t, float32_t, 1.875F, 0.007080078125F, 0.078125F>;
using smootherstep_double_test_t
    = polynomial_contract_test_t<smootherstep_t, float64_t, 1.875, 0.007080078125, 0.078125>;
using smootheststep_float_test_t
    = polynomial_contract_test_t<smootheststep_t, float32_t, 2.1875F, 0.00399017333984375F, 0.068359375F>;
using smootheststep_double_test_t
    = polynomial_contract_test_t<smootheststep_t, float64_t, 2.1875, 0.00399017333984375, 0.068359375>;

static_assert(sizeof(smoothstep_double_test_t) == 1);
static_assert(sizeof(smoothstep_float_test_t) == 1);
static_assert(sizeof(smootherstep_double_test_t) == 1);
static_assert(sizeof(smootherstep_float_test_t) == 1);
static_assert(sizeof(smootheststep_double_test_t) == 1);
static_assert(sizeof(smootheststep_float_test_t) == 1);

} // namespace
} // namespace crv::shaping::transitions
