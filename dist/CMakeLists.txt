# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frank Secilia
#
# Distribution, Ops
#
# This file manages system integration, like running dkms, loading the module at startup, and running a one shot config.

# ---------------------------------------------------------------------------------------------------------------------
# Generated Files
# ---------------------------------------------------------------------------------------------------------------------

# dkms
set(dkms dkms.conf)
set(dkms_staged ${staging_root}/${dkms})
configure_file(${dkms}.in ${dkms_staged} @ONLY)

# boot-time module loader
set(load_module load-module.conf.in)
set(load_module_staged ${staging_root}/${project_name_dashes}.conf)
configure_file(${load_module} ${load_module_staged} @ONLY)

# systemd one-shot boot time restore to configure driver
set(restore_service restore.service.in)
set(restore_service_staged ${staging_root}/${project_name_dashes}-restore.service)
configure_file(${restore_service} ${restore_service_staged} @ONLY)

# udev rules
set(udev udev.rules)
set(udev_staged ${staging_root}/99-${project_name_dashes}.rules)
configure_file(${udev}.in ${udev_staged} @ONLY)

# ---------------------------------------------------------------------------------------------------------------------
# Installation
# ---------------------------------------------------------------------------------------------------------------------

# define destinations
set(load_module_dst_dir lib/modules-load.d)
set(udev_dst_dir lib/udev/rules.d)
set(restore_service_dst lib/systemd/system)

# install generated files
install(FILES ${dkms_staged} DESTINATION ${dkms_dst_dir} COMPONENT dkms)
install(FILES ${load_module_staged} DESTINATION ${load_module_dst_dir} COMPONENT dkms)
install(FILES ${udev_staged} DESTINATION ${udev_dst_dir} COMPONENT dkms)
install(FILES ${restore_service_staged} DESTINATION ${restore_service_dst} COMPONENT dkms)

