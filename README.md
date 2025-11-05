# Curves - Modern Mouse Acceleration

Control Your Mouse Sensitivity with Speed

Curves is a kernel-mode input handler/filter driver that controls your mouse sensitivity by how fast you move it. When moving the mouse slowly, it has low sensitivity. When moving it quickly, it has high sensitivity. The transition between them is smooth. This gives you high precision at low speed while still being able to cover 3 monitors without lifting your hand. The feeling is unique, but intuitive.

## Licensing

This project uses a dual-license model:

### 1. User-Space & Shared Library: MIT License
  - This includes the common c library (src/curves/kernel/lib), c++ library (src/curves/lib), loader (src/curves/bin/loader), config (src/curves/bin/config), the cmake build, and all config files.
  - These components are licensed under the MIT License.
  - See LICENSE.
### 2. Kernel Driver: GPLv2-or-later
  - This includes the driver and everything used to build it (src/curves/kernel/driver).
  - The driver is a derivative work of the Linux kernel and is licensed under the GPL, version 2 or any later version.
  - See src/curves/kernel/driver/COPYING.

An SPDX-License-Identifier identifies files appropriately.
