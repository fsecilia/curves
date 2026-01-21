# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frank Secilia
#
# Distribution, Ops
#
# This file manages system integration, like running dkms, loading the module at startup, and running a one shot config.

set(dist_gen_root ${CMAKE_CURRENT_BINARY_DIR})

# ---------------------------------------------------------------------------------------------------------------------
# Generated Files
# ---------------------------------------------------------------------------------------------------------------------

# dkms
set(dkms dkms.conf)
set(dkms_gen ${dist_gen_root}/${dkms})
configure_file(${dkms}.in ${dkms_gen} @ONLY)

# boot-time module loader
set(load_module load-module.conf)
set(load_module_gen ${dist_gen_root}/${project_name_dashes}.conf)
configure_file(${load_module}.in ${load_module_gen} @ONLY)

# systemd one-shot boot time restore to configure driver
set(restore_service restore.service)
set(restore_service_gen ${dist_gen_root}/${project_name_dashes}-restore.service)
configure_file(${restore_service}.in ${restore_service_gen} @ONLY)

# udev rules
set(udev udev.rules)
set(udev_gen ${dist_gen_root}/99-${project_name_dashes}.rules)
configure_file(${udev}.in ${udev_gen} @ONLY)

# ---------------------------------------------------------------------------------------------------------------------
# Installation
# ---------------------------------------------------------------------------------------------------------------------

# define destinations
set(load_module_dst_dir lib/modules-load.d)
set(udev_dst_dir lib/udev/rules.d)
set(restore_service_dst lib/systemd/system)

# install generated files
install(FILES ${dkms_gen} DESTINATION ${dkms_dst_dir} COMPONENT dkms)
install(FILES ${load_module_gen} DESTINATION ${load_module_dst_dir} COMPONENT dkms)
install(FILES ${udev_gen} DESTINATION ${udev_dst_dir} COMPONENT dkms)
install(FILES ${restore_service_gen} DESTINATION ${restore_service_dst} COMPONENT dkms)

