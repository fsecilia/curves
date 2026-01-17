# SPDX-License-Identifier: MIT
# Copyright (c) 2025 Frank Secilia

src := driver

include $(src)/Kbuild

print-curves-common-sources:
	@echo $(patsubst %.o,$(src)/%.c,$(curves-common-objs))

print-curves-driver-sources:
	@echo $(patsubst %.o,$(src)/%.c,$(curves-driver-objs))
