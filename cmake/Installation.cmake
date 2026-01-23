# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frank Secilia
#
# This file declares common installation metadata needed by both project installers in src and packaging ops in dist.
# These are the naming contracts they share to ensure they refer to the same paths and files.

# determine install dirs a configure time
include(GNUInstallDirs)

# suppress "up-to-date" message cmake prints for every installed file
set(CMAKE_INSTALL_MESSAGE NEVER)

# define long names for build output
set(project_name_underscores "${PROJECT_NAME}_mouse_acceleration")        # internal filenames, like the .ko
string(REPLACE "_" "-" project_name_dashes "${project_name_underscores}") # external filenames, like the config ui

# define dkms relative install path
set(dkms_dst_dir "src/${project_name_dashes}-${PROJECT_VERSION}")
