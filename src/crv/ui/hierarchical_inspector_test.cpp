// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "hierarchical_inspector.hpp"
#include <crv/reflection/param.hpp>
#include <crv/test/test.hpp>
#include <map>
#include <sstream>

namespace crv {
namespace {

using reflection::param_t;

struct hierarchical_inspector_test_t : Test
{
    using property_map_t = std::map<std::string, std::string>;
    property_map_t map;

    struct callback_t
    {
        property_map_t& map;
        auto operator()(std::string_view path, auto const& param) const
        {
            auto stream = std::ostringstream{};
            stream << param.value();
            map.emplace(std::string{path}, stream.str());
        }
    };

    using sut_t = hierarchical_inspector_t<callback_t>;
    using factory_t = hierarchical_inspector_factory_t;
    sut_t sut = factory_t{}(callback_t{map});
};

//
// flat
//

struct hierarchical_inspector_test_flat_topology_t : hierarchical_inspector_test_t
{
    struct topology_t
    {
        param_t<std::string> param1{"param1", "param1_value"};
        param_t<std::string> param2{"param2", "param2_value"};

        template <typename self_t, typename inspector_t>
        constexpr auto reflect(this self_t&& self, inspector_t&& inspector) -> decltype(auto)
        {
            inspector.inspect(self.param1);
            inspector.inspect(self.param2);
            return std::forward<inspector_t>(inspector);
        }
    };
    topology_t model{};
};

TEST_F(hierarchical_inspector_test_flat_topology_t, handles_flat_structure)
{
    model.reflect(sut);

    EXPECT_EQ(map.size(), 2);
    EXPECT_EQ(map["param1"], "param1_value");
    EXPECT_EQ(map["param2"], "param2_value");
}

//
// single nested
//

struct hierarchical_inspector_test_nested_topology_t : hierarchical_inspector_test_t
{
    struct topology_t
    {
        struct child_t
        {
            param_t<std::string> param{"child_param", "child_value"};

            template <typename self_t, typename inspector_t>
            constexpr auto reflect(this self_t&& self, inspector_t&& inspector) -> decltype(auto)
            {
                inspector.inspect(self.param);
                return std::forward<inspector_t>(inspector);
            }
        };
        child_t child;

        template <typename self_t, typename inspector_t>
        constexpr auto reflect(this self_t&& self, inspector_t&& inspector) -> decltype(auto)
        {
            inspector.inspect_section("child", [&](auto&& section) { self.child.reflect(section); });
            return std::forward<inspector_t>(inspector);
        }
    };
    topology_t model{};
};

TEST_F(hierarchical_inspector_test_nested_topology_t, handles_single_nested_structure)
{
    model.reflect(sut);

    EXPECT_EQ(map.size(), 1);
    EXPECT_EQ(map["child/child_param"], "child_value");
}

//
// deep
//

struct hierarchical_inspector_test_deep_topology_t : hierarchical_inspector_test_t
{
    struct topology_t
    {
        struct outer_t
        {
            struct middle_t
            {
                struct inner_t
                {
                    param_t<std::string> param{"inner_param", "inner_value"};

                    template <typename self_t, typename inspector_t>
                    constexpr auto reflect(this self_t&& self, inspector_t&& inspector) -> decltype(auto)
                    {
                        inspector.inspect(self.param);
                        return std::forward<inspector_t>(inspector);
                    }
                };
                inner_t inner;

                template <typename self_t, typename inspector_t>
                constexpr auto reflect(this self_t&& self, inspector_t&& inspector) -> decltype(auto)
                {
                    inspector.inspect_section(
                        "inner", [&](auto&& section_inspector) { self.inner.reflect(section_inspector); });
                    return std::forward<inspector_t>(inspector);
                }
            };
            middle_t middle;

            template <typename self_t, typename inspector_t>
            constexpr auto reflect(this self_t&& self, inspector_t&& inspector) -> decltype(auto)
            {
                inspector.inspect_section(
                    "middle", [&](auto&& section_inspector) { self.middle.reflect(section_inspector); });
                return std::forward<inspector_t>(inspector);
            }
        };
        outer_t outer;

        template <typename self_t, typename inspector_t>
        constexpr auto reflect(this self_t&& self, inspector_t&& inspector) -> decltype(auto)
        {
            inspector.inspect_section(
                "outer", [&](auto&& section_inspector) { self.outer.reflect(section_inspector); });
            return std::forward<inspector_t>(inspector);
        }
    };
    topology_t model{};
};

TEST_F(hierarchical_inspector_test_deep_topology_t, handles_deeply_nested_structure)
{
    model.reflect(sut);

    EXPECT_EQ(map.size(), 1);
    EXPECT_EQ(map["outer/middle/inner/inner_param"], "inner_value");
}

//
// mixed_sibling
//

struct hierarchical_inspector_test_mixed_topology_t : hierarchical_inspector_test_t
{
    struct topology_t
    {
        param_t<std::string> leading_param{"leading_param", "leading_value"};

        struct nested_t
        {
            param_t<std::string> nested_param{"nested_param", "nested_value"};

            template <typename self_t, typename inspector_t>
            constexpr auto reflect(this self_t&& self, inspector_t&& section_inspector) -> decltype(auto)
            {
                section_inspector.inspect(self.nested_param);
                return std::forward<inspector_t>(section_inspector);
            }
        };
        nested_t nested;

        param_t<std::string> trailing_param{"trailing_param", "trailing_value"};

        template <typename self_t, typename inspector_t>
        constexpr auto reflect(this self_t&& self, inspector_t&& inspector) -> decltype(auto)
        {
            inspector.inspect(self.leading_param);
            inspector.inspect_section(
                "nested", [&](auto&& section_inspector) { self.nested.reflect(section_inspector); });
            inspector.inspect(self.trailing_param);
            return std::forward<inspector_t>(inspector);
        }
    };
    topology_t model;
};

TEST_F(hierarchical_inspector_test_mixed_topology_t, restores_path_after_nested_section)
{
    model.reflect(sut);

    EXPECT_EQ(map.size(), 3);
    EXPECT_EQ(map["leading_param"], "leading_value");
    EXPECT_EQ(map["nested/nested_param"], "nested_value");
    EXPECT_EQ(map["trailing_param"], "trailing_value");
}

//
// empty
//

struct hierarchical_inspector_test_empty_topology_t : hierarchical_inspector_test_t
{
    struct topology_t
    {
        param_t<std::string> preceding{"preceding_param", "preceding_value"};

        struct empty_t
        {
            template <typename self_t, typename inspector_t>
            constexpr auto reflect(this self_t&&, inspector_t&& inspector) -> decltype(auto)
            {
                return std::forward<inspector_t>(inspector);
            }
        };
        empty_t empty;

        param_t<std::string> succeeding{"succeeding_param", "succeeding_value"};

        template <typename self_t, typename inspector_t>
        constexpr auto reflect(this self_t&& self, inspector_t&& inspector) -> decltype(auto)
        {
            inspector.inspect(self.preceding);
            inspector.inspect_section(
                "empty_section", [&](auto&& section_inspector) { self.empty.reflect(section_inspector); });
            inspector.inspect(self.succeeding);
            return std::forward<inspector_t>(inspector);
        }
    };
    topology_t model{};
};

TEST_F(hierarchical_inspector_test_empty_topology_t, ignores_empty_sections_without_corrupting_path)
{
    model.reflect(sut);

    EXPECT_EQ(map.size(), 2);
    EXPECT_EQ(map["preceding_param"], "preceding_value");
    EXPECT_EQ(map["succeeding_param"], "succeeding_value");
}

} // namespace
} // namespace crv
