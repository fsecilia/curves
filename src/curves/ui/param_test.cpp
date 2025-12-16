// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2025 Frank Secilia
*/

#include <curves/testing/test.hpp>
#include <curves/ui/param.hpp>

namespace curves {
namespace {

struct IntParamTest : Test {
  using Value = int_t;

  struct MockVisitor {
    auto operator()(std::string_view name, Value& value) const noexcept
        -> void {
      on_call_reference(name, value);
    }

    auto operator()(std::string_view name, const Value& value) const noexcept
        -> void {
      on_call_value(name, value);
    }

    MOCK_METHOD(void, on_call_reference, (std::string_view, Value&),
                (const, noexcept));
    MOCK_METHOD(void, on_call_value, (std::string_view, Value),
                (const, noexcept));
    MOCK_METHOD(void, on_clamp, (std::string_view, Value, Value, Value, Value),
                (const, noexcept));

    virtual ~MockVisitor() = default;
  };
  StrictMock<MockVisitor> mock_visitor;

  using Sut = Param<Value>;

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

TEST_F(IntParamTest, const_reflect_passes_correct_values) {
  const auto sut = Sut{name, value, min, max};

  EXPECT_CALL(mock_visitor, on_call_value(name, value));

  sut.reflect(mock_visitor);
}

TEST_F(IntParamTest, mutable_reflect_allows_mutation) {
  auto sut = Sut{name, value, min, max};

  const Value new_value = 17;
  EXPECT_CALL(mock_visitor, on_call_reference(name, Eq(value)))
      .WillOnce(SetArgReferee<1>(new_value));

  sut.reflect(mock_visitor);

  ASSERT_EQ(new_value, sut.value());
}

TEST_F(IntParamTest, validate_clamps_min_and_reports) {
  const auto unclamped = min - 1;
  auto sut = Sut{name, unclamped, min, max};

  EXPECT_CALL(mock_visitor, on_clamp(name, unclamped, min, max, min));

  sut.validate(mock_visitor);

  ASSERT_EQ(min, sut.value());
}

TEST_F(IntParamTest, validate_clamps_max_and_reports) {
  const auto unclamped = max + 1;
  auto sut = Sut{name, unclamped, min, max};

  EXPECT_CALL(mock_visitor, on_clamp(name, unclamped, min, max, max));

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

}  // namespace
}  // namespace curves
