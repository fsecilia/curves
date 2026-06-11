// SPDX-License-Identifier: MIT
// Copyright (C) 2026 Frank Secilia

// Float-specific numeric value edit control.

import QtQuick

NumericField {
    decimals: 3
    validator: DoubleValidator { bottom: low; top: high }
    stepFunction: function (value, direction) {
        let absVal = Math.abs(value)
        let step = 0.01
        if (absVal >= 0.01) {
            let magnitude = Math.floor(Math.log10(absVal))
            step = Math.pow(10, magnitude - 1)
            if (direction < 0 && Math.abs(Math.pow(10, magnitude) - absVal) < 0.000001)
                step = Math.pow(10, magnitude - 2)
        }
        let next = value + (step * direction)
        let clamped = Math.min(Math.max(next, low), high)
        return Number(clamped.toFixed(3))
    }
}
