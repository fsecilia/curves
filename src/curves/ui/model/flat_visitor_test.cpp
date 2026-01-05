// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2025 Frank Secilia
*/

#include "flat_visitor.hpp"
#include <curves/testing/test.hpp>
#include <curves/config/param.hpp>
#include <string>
#include <vector>

namespace curves {
namespace {

struct FlatVisitorTest : Test {
  struct InnerConfig {
    Param<double> alpha{"Alpha", 1.0, 0.0, 10.0};
    Param<double> beta{"Beta", 2.0, 0.0, 10.0};

    auto reflect(this auto&& self, auto&& visitor) -> void {
      self.alpha.reflect(visitor);
      self.beta.reflect(visitor);
    }
  };

  struct OuterConfig {
    Param<double> gamma{"Gamma", 3.0, 0.0, 10.0};
    InnerConfig inner;

    auto reflect(this auto&& self, auto&& visitor) -> void {
      self.gamma.reflect(visitor);
      visitor.visit_section("inner", [&](auto&& section_visitor) {
        self.inner.reflect(section_visitor);
      });
    }
  };

  using NameVector = std::vector<std::string>;
};

TEST_F(FlatVisitorTest, visits_all_params_in_flat_structure) {
  InnerConfig config;
  NameVector visited_names;

  auto visitor = FlatVisitor{
      [&](auto& param) { visited_names.emplace_back(param.name()); }};

  config.reflect(visitor);

  const auto expected = NameVector{"Alpha", "Beta"};
  ASSERT_EQ(expected, visited_names);
}

TEST_F(FlatVisitorTest, flattens_nested_structure) {
  OuterConfig config;
  NameVector visited_names;

  auto visitor = FlatVisitor{
      [&](auto& param) { visited_names.emplace_back(param.name()); }};

  config.reflect(visitor);

  const auto expected = NameVector{"Gamma", "Alpha", "Beta"};
  ASSERT_EQ(expected, visited_names);
}

TEST_F(FlatVisitorTest, allows_mutation_through_callback) {
  InnerConfig config;

  auto visitor = FlatVisitor{[](auto& param) {
    // Double every value
    param.value(param.value() * 2.0);
  }};

  config.reflect(visitor);

  EXPECT_DOUBLE_EQ(2.0, config.alpha.value());
  EXPECT_DOUBLE_EQ(4.0, config.beta.value());
}

TEST_F(FlatVisitorTest, works_with_const_config) {
  const InnerConfig config;
  std::vector<double> visited_values;

  auto visitor = FlatVisitor{
      [&](const auto& param) { visited_values.push_back(param.value()); }};

  config.reflect(visitor);

  const auto expected = std::vector{1.0, 2.0};
  ASSERT_EQ(expected, visited_values);
}

// Test with enum param
enum class TestEnum { kFirst, kSecond };

}  // namespace
}  // namespace curves

template <>
struct curves::EnumReflection<curves::TestEnum> {
  [[maybe_unused]] static constexpr auto map =
      curves::sequential_name_map<curves::TestEnum>("first", "second");
};

namespace curves {
namespace {

struct FlatVisitorTestEnum : FlatVisitorTest {
  struct ConfigWithEnum {
    Param<double> value{"Value", 1.0, 0.0, 10.0};
    Param<TestEnum> mode{"Mode", TestEnum::kFirst};

    auto reflect(this auto&& self, auto&& visitor) -> void {
      std::forward<decltype(self)>(self).value.reflect(
          std::forward<decltype(visitor)>(visitor));
      std::forward<decltype(self)>(self).mode.reflect(
          std::forward<decltype(visitor)>(visitor));
    }
  };
};

TEST_F(FlatVisitorTestEnum, handles_mixed_param_types) {
  ConfigWithEnum config;
  NameVector visited_names;

  auto visitor = FlatVisitor{
      [&](auto& param) { visited_names.emplace_back(param.name()); }};

  config.reflect(visitor);

  const auto expected = NameVector{"Value", "Mode"};
  ASSERT_EQ(expected, visited_names);
}

}  // namespace
}  // namespace curves
