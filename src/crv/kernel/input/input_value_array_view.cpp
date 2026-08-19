// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "input_value_array_view.hpp"

namespace crv {
namespace {

CRV_SAME_LAYOUT(input_value_t, crv_input_value_t);
CRV_MEMBER_SAME_LAYOUT_INTEGER(input_value_t, crv_input_value_t, type);
CRV_MEMBER_SAME_LAYOUT_INTEGER(input_value_t, crv_input_value_t, code);
CRV_MEMBER_SAME_LAYOUT_INTEGER(input_value_t, crv_input_value_t, value);

} // namespace
} // namespace crv
