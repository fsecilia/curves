// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \brief linux input handler lifecycle
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <linux/input.h>

struct crv_control_t;

struct crv_input_handler_t
{
    struct input_handler input;
    struct crv_control_t* control;
};

int crv_input_handler_register(struct crv_input_handler_t* handler, struct crv_control_t* control);
void crv_input_handler_unregister(struct crv_input_handler_t* handler);
