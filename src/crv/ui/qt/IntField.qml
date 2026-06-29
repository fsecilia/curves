// SPDX-License-Identifier: MIT
// Copyright (C) 2026 Frank Secilia

// Integer-specific numeric value edit control.

import QtQuick

NumericField {
    decimals: 0
    validator: IntValidator { bottom: min; top: max }
    stepFunction: function (value, direction) { return Math.min(Math.max(Math.round(value) + direction, min), max) }
}
