// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \brief temporary test module to make sure c++ builds freestanding and can talk to c
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

int reference_cxx(void);

#ifdef __cplusplus
} // extern "C" {
#endif
