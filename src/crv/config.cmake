# SPDX-License-Identifier: GPL-2.0+ OR MIT
# Copyright (c) 2026 Frank Secilia
#
# common config shared between kernel module and user-mode library

function(configure_shared target visibility)
    set(root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}")

    target_sources("${target}" "${visibility}"
        "${root}/algorithm.hpp"
        "${root}/bitwise_enum.hpp"
        "${root}/kernel/abi.h"
        "${root}/lib.hpp"
        "${root}/math/abs.hpp"
        "${root}/math/cmp.hpp"
        "${root}/math/division/concepts.hpp"
        "${root}/math/division/divider.hpp"
        "${root}/math/division/hardware_divider.hpp"
        "${root}/math/division/io.hpp"
        "${root}/math/division/qr_pair.hpp"
        "${root}/math/division/shifted_int_divider.hpp"
        "${root}/math/division/wide_divider.hpp"
        "${root}/math/fixed/exp2_neg_m1.cpp"
        "${root}/math/fixed/exp2_neg_m1.hpp"
        "${root}/math/fixed/exp2.hpp"
        "${root}/math/fixed/fixed.hpp"
        "${root}/math/fixed/fma.hpp"
        "${root}/math/fixed/uabs.hpp"
        "${root}/math/int_traits.hpp"
        "${root}/math/integer.hpp"
        "${root}/math/inverse.hpp"
        "${root}/math/limits.hpp"
        "${root}/math/linear.hpp"
        "${root}/math/rounding_mode.hpp"
        "${root}/math/saturate_cast.hpp"
        "${root}/math/scalar_traits.hpp"
        "${root}/math/shifter.hpp"
        "${root}/pipeline/filters/one_euro/filter.hpp"
        "${root}/prefetcher.hpp"
        "${root}/ranges.hpp"
        "${root}/spline/pipeline_config.hpp"
        "${root}/spline/segment_locator.hpp"
        "${root}/spline/segment.hpp"
        "${root}/spline/spline.hpp"
        "${root}/spline/tangent_extension.hpp"
        "${root}/traits.hpp"
    )

    target_compile_options("${target}" "${visibility}"
        $<$<COMPILE_LANGUAGE:CXX>:
            -Wall
            -Werror
            -Wextra
            -Wno-psabi
            -Wswitch
        >
    )

    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options("${target}" "${visibility}"
            -fdiagnostics-color=always
            -fext-numeric-literals
            -ftemplate-backtrace-limit=1

            -Wno-changes-meaning
        )
    endif()

    if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        target_compile_options("${target}" "${visibility}"
            -fcolor-diagnostics
            -fsafe-buffer-usage-suggestions

            $<$<COMPILE_LANGUAGE:CXX>:
                -Weverything
                -Wno-c++20-compat
                -Wno-c++23-compat
                -Wno-c++98-compat
                -Wno-c++98-compat-pedantic
                -Wno-c++2c-compat
                -Wno-c++2c-extensions
                -Wno-c99-extensions
                -Wno-ctad-maybe-unsupported
                -Wno-deprecated-copy-with-dtor
                -Wno-deprecated-copy-with-user-provided-dtor
                -Wno-disabled-macro-expansion
                -Wno-documentation
                -Wno-documentation-unknown-command
                -Wno-missing-prototypes
                -Wno-padded
                -Wno-reserved-macro-identifier
                -Wno-sign-conversion
                -Wno-shadow
                -Wno-shadow-field
                -Wno-shadow-field-in-constructor
                -Wno-switch-default
                -Wno-switch-enum
                -Wno-unneeded-member-function
                -Wno-unsafe-buffer-usage
                -Wno-unused-function
                -Wno-unused-member-function
                -Wno-unused-template
                -Wno-weak-vtables
            >
        )
    endif()
endfunction()
