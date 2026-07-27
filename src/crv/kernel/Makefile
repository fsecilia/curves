# SPDX-License-Identifier: GPL-2.0+ OR MIT
# Copyright (c) 2026 Frank Secilia

KERNEL_RELEASE ?= $(shell uname -r)
KERNEL_DIR     ?= /lib/modules/$(KERNEL_RELEASE)/build

include $(KERNEL_DIR)/include/config/auto.conf

KBUILD_ARGS := -C "$(KERNEL_DIR)" \
               M="$(CURDIR)" \
               LDFLAGS_MODULE=--force-group-allocation \
               CONFIG_DEBUG_INFO_BTF_MODULES=

ifeq ($(CONFIG_CC_IS_CLANG),y)
KBUILD_ARGS += LLVM=1
endif

.PHONY: default modules clean install modules_install

default: modules

install: modules_install

modules clean:
	+$(MAKE) $(KBUILD_ARGS) $@

modules_install:
	+$(MAKE) $(KBUILD_ARGS) INSTALL_MOD_STRIP=1 $@
