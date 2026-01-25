# Curves - Modern Mouse Acceleration

Control Mouse Sensitivity with Speed

Curves is a kernel-mode input handler/filter driver that controls your mouse sensitivity by how fast you move it. When moving the mouse slowly, it has low sensitivity. When moving it quickly, it has high sensitivity. The transition between them is smooth. This gives you high precision at low speed while still being able to cover 3 monitors without lifting your hand. The feeling is unique, but intuitive.

## Building

The build is standard cmake. Presets for debug and release using clang and gcc are provided. The build directory defaults to ./builds, but shadow builds are supported, as is overriding the path in CMakeUserPresets.json.

The install will place the config binary in `bin`, prep a dkms build of the driver in `usr/src`, place supporting udev rules in `lib/udev/rules.d`, systemd integration in `lib/systemd/system`, and the initial module load in `lib/modules-load.d`, all relative to `CMAKE_INSTALL_PREFIX`. Packages can be staged using DESTDIR.

## Packages

Prebuilt packages are available from [OBS](https://download.opensuse.org/repositories/home:/fsecilia/).

[![build result](https://build.opensuse.org/projects/home:fsecilia/packages/curves/badge.svg?type=default)](https://build.opensuse.org/package/show/home:fsecilia/curves)

## Licensing

This project uses a dual-license model:
    - Everything is released under the MIT license. See LICENSE.
    - Alternatively, the kernel module is also released under GPLv2-or-later.
    See COPYING.

An SPDX-License-Identifier identifies files appropriately.
