# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frank Secilia
#
# This file manages PKGBUILD packaging. It configures the pkgbuild script itself.

set(pkgbuild_gen_root ${CMAKE_CURRENT_BINARY_DIR}/pkgbuild)

# ---------------------------------------------------------------------------------------------------------------------
# Generated Files
# ---------------------------------------------------------------------------------------------------------------------

set(pkgbuild_spec spec)
set(pkgbuild_spec_gen ${pkgbuild_gen_root}/${pkgbuild_spec})
configure_file(${pkgbuild_spec}.in ${pkgbuild_spec_gen} @ONLY USE_SOURCE_PERMISSIONS)
