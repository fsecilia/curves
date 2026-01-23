# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frank Secilia

# find system threading library
set(THREADS_PREFER_PTHREAD_FLAG True)
find_package(Threads REQUIRED)
