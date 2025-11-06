# SPDX-License-Identifier: MIT
# Copyright (c) 2025 Frank Secilia

include driver/Kbuild

# Prints list of source files common to the kernel module and userland.
print-curves-common-sources:
	@echo $(patsubst %.o,driver/%.c,$(curves-common-objs))

# Prints list of source files specific to the kernel module.
print-curves-module-sources:
	@echo $(patsubst %.o,driver/%.c,$(curves-module-objs))
