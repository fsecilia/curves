// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/kernel/copy_to_user.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/queue/copy_to_user_copier.hpp>
#include <crv/queue/record_copier.hpp>

namespace crv {

struct composition_t
{
    using copy_to_user_copier_factory_t = copy_to_user_copier_factory_t<kernel::copy_to_user_factory_t>;

    // give the copier a concrete record to test compilation from within the kernel
    struct velocity_record_t
    {
        uint64_t timestamp_ns;
        fixed_t<int64_t, 42> velocity_counts_per_ms;
    };
    using velocity_copy_to_user_record_copier_factory_t
        = record_copier_factory_t<velocity_record_t, copy_to_user_copier_factory_t>;
};

} // namespace crv
