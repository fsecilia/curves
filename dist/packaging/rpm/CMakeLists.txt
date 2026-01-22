# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frank Secilia
#
# This file manages .rpm packaging. It configures the rpm-specific metadata.

set(rpm_gen_root ${CMAKE_CURRENT_BINARY_DIR}/rpm)

# ---------------------------------------------------------------------------------------------------------------------
# Generated Files
# ---------------------------------------------------------------------------------------------------------------------

set(rpm_spec spec)
set(rpm_spec_gen ${rpm_gen_root}/${rpm_spec})
configure_file(${rpm_spec}.in ${rpm_spec_gen} @ONLY USE_SOURCE_PERMISSIONS)
