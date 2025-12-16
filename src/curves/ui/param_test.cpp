// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2025 Frank Secilia
*/

#include <curves/testing/test.hpp>
#include <curves/ui/param.hpp>

namespace curves {
namespace {

// ----------------------------------------------------------------------------
// Param<T>
// ----------------------------------------------------------------------------

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

// ----------------------------------------------------------------------------
// Param<Enum>
// ----------------------------------------------------------------------------

enum ParamTestEnum {
  value_0,
  value_1,
  value_2,
};

}  // namespace

template <>
struct EnumTraits<ParamTestEnum> {
  static constexpr std::string_view names[] = {
      "value_0",
      "value_1",
      "value_2",
  };

  static constexpr std::string_view to_string(ParamTestEnum value) {
    const auto index = static_cast<size_t>(value);
    if (index < std::size(names)) {
      return names[index];
    }
    return "unknown";
  }

  static constexpr std::optional<ParamTestEnum> from_string(
      std::string_view s) {
    for (size_t i = 0; i < std::size(names); ++i) {
      if (names[i] == s) {
        return static_cast<ParamTestEnum>(i);
      }
    }
    return std::nullopt;
  }
};

namespace {

struct EnumParamTest : Test {
  using Value = ParamTestEnum;

  static inline const auto invalid_enum_value_string =
      "773ed36c01c54b06b13e4bb835b1da59";

  struct MockVisitor {
    auto operator()(std::string_view name,
                    std::string_view& value_string) const noexcept -> void {
      on_call_reference(name, value_string);
    }

    auto operator()(std::string_view name,
                    const std::string_view& value_string) const noexcept
        -> void {
      on_call_value(name, value_string);
    }

    MOCK_METHOD(void, on_call_reference, (std::string_view, std::string_view&),
                (const, noexcept));
    MOCK_METHOD(void, on_call_value, (std::string_view, std::string_view),
                (const, noexcept));
    MOCK_METHOD(void, report_error, (std::string_view), (const, noexcept));

    virtual ~MockVisitor() = default;
  };
  StrictMock<MockVisitor> mock_visitor;

  using Sut = Param<Value>;

  const std::string_view name = std::string_view{"name"};
  const Value value = Value::value_1;
};

TEST_F(EnumParamTest, properties_initialized_correctly) {
  const auto sut = Sut{name, value};

  ASSERT_EQ(name, sut.name());
  ASSERT_EQ(value, sut.value());
}

TEST_F(EnumParamTest, const_reflect_passes_correct_values) {
  const auto sut = Sut{name, value};

  EXPECT_CALL(mock_visitor,
              on_call_value(name, EnumTraits<Value>::to_string(value)));

  sut.reflect(mock_visitor);
}

TEST_F(EnumParamTest, mutable_reflect_allows_mutation_with_valid_string) {
  auto sut = Sut{name, value};

  const Value new_value = Value::value_2;
  EXPECT_CALL(mock_visitor,
              on_call_reference(name, Eq(EnumTraits<Value>::to_string(value))))
      .WillOnce(SetArgReferee<1>(EnumTraits<Value>::to_string(new_value)));

  sut.reflect(mock_visitor);

  ASSERT_EQ(new_value, sut.value());
}

TEST_F(EnumParamTest,
       mutable_reflect_reports_invalid_string_when_callback_available) {
  auto sut = Sut{name, value};

  // Set proxy to a string that doesn't correspond to any enum value.
  EXPECT_CALL(mock_visitor,
              on_call_reference(name, Eq(EnumTraits<Value>::to_string(value))))
      .WillOnce(SetArgReferee<1>(invalid_enum_value_string));

  // Error callback will be called.
  EXPECT_CALL(mock_visitor, report_error(HasSubstr(invalid_enum_value_string)));

  sut.reflect(mock_visitor);

  // Value remains unchanged.
  ASSERT_EQ(value, sut.value());
}

TEST_F(EnumParamTest,
       mutable_reflect_ignores_invalid_string_when_callback_unavailable) {
  auto sut = Sut{name, value};

  sut.reflect([&](auto, auto& proxy) { proxy = invalid_enum_value_string; });

  ASSERT_EQ(value, sut.value());
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
