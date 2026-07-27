# SPDX-License-Identifier: GPL-2.0+ OR MIT
# Copyright (c) 2026 Frank Secilia

KERNEL_RELEASE ?= $(shell uname -r)
KERNEL_DIR     ?= /lib/modules/$(KERNEL_RELEASE)/build

KBUILD_ARGS := -C "$(KERNEL_DIR)" \
               M="$(CURDIR)" \
               LDFLAGS_MODULE=--force-group-allocation \
               CONFIG_DEBUG_INFO_BTF_MODULES=

.PHONY: default modules clean install modules_install

default: modules

install: modules_install

modules modules_install clean:
	+$(MAKE) $(KBUILD_ARGS) $@
