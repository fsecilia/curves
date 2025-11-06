# SPDX-License-Identifier: GNU
# Copyright (c) 2025 Frank Secilia

obj-m += curves.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
