# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frank Secilia
#
# declares all cmake policy decisions in one place

# Don't repeat libraries on build command lines.
if (POLICY CMP0156)
    cmake_policy(SET CMP0156 NEW)
endif()
