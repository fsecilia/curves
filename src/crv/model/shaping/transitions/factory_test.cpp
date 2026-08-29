// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "factory.hpp"
#include <crv/test/test.hpp>
#include <concepts>
#include <optional>

namespace crv::shaping::transitions {
namespace {

struct transition_factory_test_t : Test
{
    struct quadrature_receipt_t
    {
        float_t achieved_error;

        constexpr auto operator==(quadrature_receipt_t const&) const noexcept -> bool = default;
    };

    struct nast_t
    {
        int_t value;
    };

    using sut_t = transition_factory_t<nast_t, quadrature_receipt_t>;
    sut_t sut{typename sut_t::nast_product_t{
        .transition = {.value = 53},
        .quadrature_receipt = quadrature_receipt_t{.achieved_error = 7.11},
    }};

    template <typename expected_t> auto selects(continuity_t continuity) const -> bool
    {
        return sut(continuity, []<typename product_t>(product_t) -> bool {
            return std::same_as<typename product_t::transition_t, expected_t>;
        });
    }
};

TEST_F(transition_factory_test_t, c1_selects_smoothstep)
{
    EXPECT_TRUE(selects<smoothstep_t>(continuity_t::c1));
}

TEST_F(transition_factory_test_t, c2_selects_smootherstep)
{
    EXPECT_TRUE(selects<smootherstep_t>(continuity_t::c2));
}

TEST_F(transition_factory_test_t, c3_selects_smootheststep)
{
    EXPECT_TRUE(selects<smootheststep_t>(continuity_t::c3));
}

TEST_F(transition_factory_test_t, cinfinity_selects_nast)
{
    EXPECT_TRUE(selects<nast_t>(continuity_t::cinfinity));
}

TEST_F(transition_factory_test_t, polynomial_has_no_quadrature_receipt)
{
    auto const has_receipt = sut(continuity_t::c3,
        []<typename product_t>(product_t product) -> bool { return product.quadrature_receipt.has_value(); });

    EXPECT_FALSE(has_receipt);
}

TEST_F(transition_factory_test_t, cinfinity_forwards_quadrature_receipt)
{
    auto const receipt = sut(
        continuity_t::cinfinity, []<typename product_t>(product_t product) -> std::optional<quadrature_receipt_t> {
            return product.quadrature_receipt;
        });

    EXPECT_EQ(receipt, quadrature_receipt_t{.achieved_error = 7.11});
}

TEST_F(transition_factory_test_t, copies_retained_nast_value)
{
    auto const actual = sut(continuity_t::cinfinity, []<typename product_t>(product_t product) -> std::optional<int_t> {
        if constexpr (std::same_as<typename product_t::transition_t, nast_t>) return product.transition.value;
        return std::nullopt;
    });

    EXPECT_EQ(actual, 53);
}

} // namespace
} // namespace crv::shaping::transitions
