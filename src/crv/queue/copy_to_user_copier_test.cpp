// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "copy_to_user_copier.hpp"
#include <crv/test/test.hpp>
#include <gmock/gmock.h>

namespace crv {
namespace {

struct fixture_t
{
    std::array<std::byte, 64> dst_storage{};
    void* dst = dst_storage.data();
    static constexpr int fault_error = -3; // arbitrary
};

//
// copy_to_user_copier_t
//

struct copy_to_user_copier_test_t : Test, fixture_t
{
    struct mock_residual_copy_t
    {
        virtual ~mock_residual_copy_t() = default;
        MOCK_METHOD(std::size_t, call, (std::byte*, std::byte const*, std::size_t), (noexcept));
    };
    StrictMock<mock_residual_copy_t> mock_residual_copy;

    struct residual_copy_t
    {
        mock_residual_copy_t* mock = nullptr;
        auto operator()(std::byte* dst, std::span<std::byte const> src) noexcept -> std::size_t
        {
            return mock->call(dst, src.data(), src.size());
        }
    };

    std::array<std::byte, 16> src_storage{};

    using sut_t = copy_to_user_copier_t<residual_copy_t>;
    sut_t sut{dst, {&mock_residual_copy}, fault_error};
};

TEST_F(copy_to_user_copier_test_t, complete_copy_returns_source_size_and_reports_no_error)
{
    static constexpr auto dst_offset = std::size_t{7};
    auto const src = std::span<std::byte const>{src_storage}.first(11);

    EXPECT_CALL(mock_residual_copy, call(dst_storage.data() + dst_offset, src.data(), src.size())).WillOnce(Return(0));

    auto const copied = sut(dst_offset, src);

    EXPECT_EQ(src.size(), copied);
    EXPECT_EQ(0, sut.error());
}

TEST_F(copy_to_user_copier_test_t, partial_copy_returns_copied_prefix_and_reports_fault_error)
{
    static constexpr auto dst_offset = std::size_t{5};
    static constexpr auto uncopied = std::size_t{4};
    auto const src = std::span<std::byte const>{src_storage}.first(13);

    EXPECT_CALL(mock_residual_copy, call(dst_storage.data() + dst_offset, src.data(), src.size()))
        .WillOnce(Return(uncopied));

    auto const copied = sut(dst_offset, src);

    EXPECT_EQ(src.size() - uncopied, copied);
    EXPECT_EQ(fault_error, sut.error());
}

TEST_F(copy_to_user_copier_test_t, complete_residual_returns_zero_and_reports_fault_error)
{
    static constexpr auto dst_offset = std::size_t{3};
    auto const src = std::span<std::byte const>{src_storage}.first(9);

    EXPECT_CALL(mock_residual_copy, call(dst_storage.data() + dst_offset, src.data(), src.size()))
        .WillOnce(Return(src.size()));

    auto const copied = sut(dst_offset, src);

    EXPECT_EQ(0, copied);
    EXPECT_EQ(fault_error, sut.error());
}

TEST_F(copy_to_user_copier_test_t, complete_copy_may_be_followed_by_short_copy)
{
    auto const first = std::span<std::byte const>{src_storage}.first(6);
    auto const second = std::span<std::byte const>{src_storage}.subspan(6, 8);
    static constexpr auto second_uncopied = std::size_t{3};

    {
        InSequence sequence;

        EXPECT_CALL(mock_residual_copy, call(dst_storage.data(), first.data(), first.size())).WillOnce(Return(0));

        EXPECT_CALL(mock_residual_copy, call(dst_storage.data() + first.size(), second.data(), second.size()))
            .WillOnce(Return(second_uncopied));
    }

    auto const first_copied = sut(0, first);

    EXPECT_EQ(first.size(), first_copied);
    EXPECT_EQ(0, sut.error());

    auto const second_copied = sut(first.size(), second);

    EXPECT_EQ(second.size() - second_uncopied, second_copied);
    EXPECT_EQ(fault_error, sut.error());
}

#if defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG

TEST_F(copy_to_user_copier_test_t, invocation_after_short_copy_asserts)
{
    auto const src = std::span<std::byte const>{src_storage}.first(8);

    EXPECT_CALL(mock_residual_copy, call(dst_storage.data(), src.data(), src.size())).WillOnce(Return(1));

    EXPECT_EQ(src.size() - 1, sut(0, src));
    EXPECT_EQ(fault_error, sut.error());

    EXPECT_DEATH({ static_cast<void>(sut(0, src)); }, "");
}

TEST_F(copy_to_user_copier_test_t, residual_larger_than_source_asserts)
{
    struct excessive_residual_copy_t
    {
        [[nodiscard]] auto operator()(std::byte*, std::span<std::byte const> src) const noexcept -> std::size_t
        {
            return src.size() + 1;
        }
    };

    auto const src = std::span<std::byte const>{src_storage}.first(8);

    EXPECT_DEATH(([&] {
        auto copier = copy_to_user_copier_t{
            dst,
            excessive_residual_copy_t{},
            fault_error,
        };

        static_cast<void>(copier(0, src));
    }()),
        "");
}

TEST_F(copy_to_user_copier_test_t, nonnegative_fault_error_asserts)
{
    EXPECT_DEBUG_DEATH((sut_t{dst, residual_copy_t{&mock_residual_copy}, 0}), "");
}

#endif

//
// copy_to_user_copier_factory_t
//

struct copy_to_user_copier_factory_test_t : Test, fixture_t
{
    struct residual_copy_factory_t
    {
        using product_t = int_t;
        static constexpr auto expected_product = 3;

        constexpr auto operator()() const -> int_t { return expected_product; }
    };

    struct product_t
    {
        void* dst;
        residual_copy_factory_t::product_t residual_copy_factory_product;
        int fault_error;

        constexpr auto operator==(product_t const&) const noexcept -> bool = default;
    };

    using sut_t = copy_to_user_copier_factory_t<residual_copy_factory_t, product_t>;
    sut_t sut{residual_copy_factory_t{}, fault_error};
};

TEST_F(copy_to_user_copier_factory_test_t, product)
{
    auto const expected = product_t{.dst = dst,
        .residual_copy_factory_product = residual_copy_factory_t::expected_product,
        .fault_error = fault_error};

    auto const actual = sut(dst);

    EXPECT_EQ(expected, actual);
}

} // namespace
} // namespace crv
