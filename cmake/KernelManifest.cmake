# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frank Secilia
#
# manages running list of kernel modules that thread source tree
#
# Kernel modules have no concept of libraries. There's just one build that contains everything. However, most of the
# code compiled into the kernel module is also shared with the usermode library. Grouping all of the source files the
# kernel builds explicitly under the kernel directory breaks the natural library encapsulation. Instead, files compiled
# into the kernel are flagged individually and aggregated. This file manages the list of flagged files.

# global property to collect absolute paths to each file that goes into the kernel build
define_property(GLOBAL PROPERTY kernel_manifest
    BRIEF_DOCS "list of sources to include in Kbuild"
    FULL_DOCS "Contains absolute paths to all source files shared with the kernel."
)

# adds given source file arguments to both given target and kernel manifest
function(target_sources_kernel target)
    foreach(source_file ${ARGN})
        # add to user mode target
        target_sources(${target} PRIVATE "${source_file}")

        # add abs path to kernel manifest
        get_filename_component(abs_path "${source_file}" ABSOLUTE)
        set_property(GLOBAL APPEND PROPERTY kernel_manifest "${abs_path}")

        # restrict to using kernel-compatible compile options
        set_source_files_properties(${source_file}
            PROPERTIES
            COMPILE_OPTIONS "${kernel_c_flags};$<$<COMPILE_LANGUAGE:CXX>:${kernel_cxx_flags}>"
        )
    endforeach()
endfunction()
