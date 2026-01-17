// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Visitor that flattens hierarchical config traversal.

  FlatVisitor adapts a simple callback into a full visitor by transparently
  handling section traversal. The callback sees every Param in the tree without
  needing to know about the hierarchy.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <curves/config/param.hpp>
#include <functional>
#include <utility>

namespace curves {

//! Adapts a single callback into a visitor that flattens section hierarchy.
template <typename Callback>
class FlatVisitor {
 public:
  explicit FlatVisitor(Callback callback) : callback_{std::move(callback)} {}

  auto operator()(this auto&& self, auto&& param) -> void {
    std::forward<decltype(self)>(self).callback_(
        std::forward<decltype(param)>(param));
  }

  template <typename VisitSectionFunc>
  auto visit_section(std::string_view, VisitSectionFunc&& visit_section)
      -> void {
    std::invoke(std::forward<VisitSectionFunc>(visit_section), *this);
  }

 private:
  Callback callback_;
};

}  // namespace curves
