// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2025 Frank Secilia
*/

#include "param.hpp"
#include <curves/testing/test.hpp>

namespace curves {
namespace {

// ----------------------------------------------------------------------------
// Param<T>
// ----------------------------------------------------------------------------

struct IntParamTest : Test {
  using Value = int_t;
  using Sut = Param<Value>;

  struct MockVisitor {
    template <typename T>
    auto operator()(T&& param) const noexcept -> void {
      on_call(std::forward<T>(param));
    }

    MOCK_METHOD(void, on_call, (Sut&), (const, noexcept));
    MOCK_METHOD(void, on_call, (const Sut&), (const, noexcept));
    MOCK_METHOD(void, report_warning, (std::string), (const));

    virtual ~MockVisitor() = default;
  };
  StrictMock<MockVisitor> mock_visitor;

  const std::string_view name = std::string_view{"name"};
  const Value value = 3;
  const Value min = 2;
  const Value max = 4;
};

TEST_F(IntParamTest, properties_initialized_correctly) {
  const auto sut = Sut{name, value, min, max};

  ASSERT_EQ(name, sut.name());
  ASSERT_EQ(value, sut.value());
  ASSERT_EQ(min, sut.min());
  ASSERT_EQ(max, sut.max());
}

TEST_F(IntParamTest, const_reflect_passes_const_self) {
  const auto sut = Sut{name, value, min, max};

  EXPECT_CALL(mock_visitor, on_call(An<const Sut&>()));

  sut.reflect(mock_visitor);
}

TEST_F(IntParamTest, mutable_reflect_passes_mutable_self) {
  auto sut = Sut{name, value, min, max};

  EXPECT_CALL(mock_visitor, on_call(An<Sut&>()));

  sut.reflect(mock_visitor);
}

TEST_F(IntParamTest, validate_clamps_min_and_reports) {
  const auto unclamped = min - 1;
  auto sut = Sut{name, unclamped, min, max};

  const auto expected_substring = std::format("{}", unclamped);
  EXPECT_CALL(mock_visitor, report_warning(HasSubstr(expected_substring)));

  sut.validate(mock_visitor);

  ASSERT_EQ(min, sut.value());
}

TEST_F(IntParamTest, validate_clamps_max_and_reports) {
  const auto unclamped = max + 1;
  auto sut = Sut{name, unclamped, min, max};

  const auto expected_substring = std::format("{}", unclamped);
  EXPECT_CALL(mock_visitor, report_warning(HasSubstr(expected_substring)));

  sut.validate(mock_visitor);

  ASSERT_EQ(max, sut.value());
}

TEST_F(IntParamTest, validate_ignores_visitor_without_callback) {
  auto sut = Sut{name, max + 1, min, max};

  sut.validate([](auto&&...) {});

  ASSERT_EQ(max, sut.value());
}

TEST_F(IntParamTest, validate_works_without_visitor) {
  auto sut = Sut{name, max + 1, min, max};

  sut.validate();

  ASSERT_EQ(max, sut.value());
}

// ----------------------------------------------------------------------------
// Param<Enum>
// ----------------------------------------------------------------------------

enum ParamTestEnum {
  value_0,
  value_1,
  value_2,
};

struct EnumParamTest : Test {
  using Value = ParamTestEnum;
  using Sut = Param<Value>;

  struct MockVisitor {
    template <typename T>
    auto operator()(T&& param) const noexcept -> void {
      on_call(std::forward<T>(param));
    }

    MOCK_METHOD(void, on_call, (Sut&), (const, noexcept));
    MOCK_METHOD(void, on_call, (const Sut&), (const, noexcept));
    MOCK_METHOD(void, report_error, (std::string_view), (const, noexcept));

    virtual ~MockVisitor() = default;
  };
  StrictMock<MockVisitor> mock_visitor;

  const std::string_view name = std::string_view{"name"};
  const Value value = Value::value_1;
};

TEST_F(EnumParamTest, properties_initialized_correctly) {
  const auto sut = Sut{name, value};

  ASSERT_EQ(name, sut.name());
  ASSERT_EQ(value, sut.value());
}

TEST_F(EnumParamTest, const_reflect_passes_const_self) {
  const auto sut = Sut{name, value};

  EXPECT_CALL(mock_visitor, on_call(An<const Sut&>()));

  sut.reflect(mock_visitor);
}

TEST_F(EnumParamTest, mutable_reflect_passes_mutable_self) {
  auto sut = Sut{name, value};

  EXPECT_CALL(mock_visitor, on_call(An<Sut&>()));

  sut.reflect(mock_visitor);
}

TEST_F(EnumParamTest, validate_no_op) {
  auto sut = Sut{name, value};
  sut.validate([]() {});
  ASSERT_EQ(value, sut.value());
}

TEST_F(EnumParamTest, validate_works_without_visitor) {
  auto sut = Sut{name, value};
  sut.validate();
  ASSERT_EQ(value, sut.value());
}

}  // namespace
}  // namespace curves
