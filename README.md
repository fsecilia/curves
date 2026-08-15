# Curves - Modern Mouse Acceleration

Control Mouse Sensitivity with Speed

Curves is a kernel-mode input handler that controls mouse sensitivity with mouse movement speed. When moving the mouse slowly, it has low sensitivity. When moving it quickly, it has high sensitivity. The transition between high and low sensitivity is smooth. This gives high precision at low speed while still being able to cover 3 monitors using only a few inches of desk space. The feeling is unique, but intuitive.

## Requirements

. CMake 3.31.6 or newer;
. g++ 14 or Clang 17, or newer
. Qt6;
. GNU Make;
. a prepared Linux kernel build directory;
. Linux 6.12 as the oldest supported and continuously tested Kbuild baseline;
. the compiler and utilities required by that kernel;
. a linker supporting --force-group-allocation.

This project uses c++(26|2c). The frontend config ui can be built with either g++ or Clang, but the backend kernel module builds with the same compiler that was used to build the kernel. GNU Make is only required for the kernel module Kbuild. The rest of the project builds using any standard CMake generator.

The build requires Toml++ and Qt6. When building from git, Toml++ is vendored via git submodule if not available. Testing is optional and requires gtest to enable.

## Building

The build is standard cmake. Presets for debug and release using Clang and g++ are provided. The build directory defaults to ./builds, but shadow builds are supported, as is overriding the path in CMakeUserPresets.json.

The install will place the config binary in `bin`, prep a dkms build of the kernel module in `usr/src`, place supporting udev rules in `lib/udev/rules.d`, systemd integration in `lib/systemd/system`, and the initial module load in `lib/modules-load.d`, all relative to `CMAKE_INSTALL_PREFIX`. Packages can be staged using DESTDIR.

## Packages

Prebuilt packages are available from [OBS](https://download.opensuse.org/repositories/home:/fsecilia/).

[![build result](https://build.opensuse.org/projects/home:fsecilia/packages/curves/badge.svg?type=default)](https://build.opensuse.org/package/show/home:fsecilia/curves)

## Licensing

This repository contains code available under the MIT License (see LICENSE) or dual-licensed as MIT/GPL-2.0-or-later (see COPYING).
 - All code exclusive to user mode, such as the config app and libraries, are distributed strictly as MIT.
 - All code used in the kernel module build, including code exclusive to the kernel module and code shared between    the kernel module and user mode, are distributed as dual MIT/GPL. The kernel module binary is GPL-compliant.

Individual source files contain SPDX-License-Identifier tag indicating their specific license.
