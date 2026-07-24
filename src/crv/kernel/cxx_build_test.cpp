// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

extern "C" {
#include "cxx_build_test.h"
#include <linux/errno.h>
} // extern "C"

#include <crv/kernel/composition.hpp>
#include <crv/math/fixed/fixed.hpp>

#include <array>
#include <cstddef>

namespace crv {

using q47_16i_t = fixed_t<int, 16>;
using q63_0i_t = fixed_t<int64_t, 0>;

template struct fixed_t<int, 16>;
template struct fixed_t<int64_t, 0>;

} // namespace crv

extern "C" int reference_cxx(void)
{
    using namespace crv;

    auto dst_storage = std::array<std::byte, 64>{};

    auto velocity_copy_to_user_record_copier_factory = composition_t::velocity_copy_to_user_record_copier_factory_t{
        copy_to_user_copier_factory_t{kernel::copy_to_user_factory_t{}, -EFAULT}};
    auto velocity_copy_to_user_record_copier = velocity_copy_to_user_record_copier_factory(dst_storage.data());

    auto const one = q63_0i_t{1};
    return q47_16i_t::convert(one).value + velocity_copy_to_user_record_copier.error();
}
