// SPDX-License-Identifier: MIT
// Copyright (C) 2026 Frank Secilia

// Numeric value edit control.

import QtQuick
import QtQuick.Controls

// Abstract numeric text edit control. Adds fields `decimals`, `validator`, and `stepFunction`.
TextField {
    id: inputField
    horizontalAlignment: TextInput.AlignRight
    color: sysPalette.text

    // set by the concrete field
    property int decimals: 0
    property var stepFunction // (value, direction) -> next clamped value

    SystemPalette { id: sysPalette; colorGroup: SystemPalette.Active }

    readonly property real low: model.minVal !== undefined ? model.minVal : -999999.0
    readonly property real high: model.maxVal !== undefined ? model.maxVal :  999999.0
    readonly property string display: Number(model.value).toLocaleString(Qt.locale(), 'f', decimals)

    text: display

    function commit() {
        let parsed
        try { parsed = Number.fromLocaleString(Qt.locale(), text) }
        catch (e) { text = display; return }
        model.value = Math.min(Math.max(parsed, low), high)
        text = display
    }

    onDisplayChanged: text = display
    onActiveFocusChanged: if (!activeFocus && !acceptableInput) text = display
    onEditingFinished: commit()

    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) { commit(); event.accepted = true }
        else if (event.matches(StandardKey.Undo)) { undoStack.undo(); event.accepted = true }
        else if (event.matches(StandardKey.Redo)) { undoStack.redo(); event.accepted = true }
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.NoButton
        onWheel: (wheel) => {
            if (!inputField.activeFocus) { wheel.accepted = false; return }
            let direction = wheel.angleDelta.y > 0 ? 1 : -1
            let base
            try { base = Number.fromLocaleString(Qt.locale(), inputField.text) }
            catch (e) { base = Number(model.value) }
            sectionModel.on_wheel(index, inputField.stepFunction(base, direction))
            wheel.accepted = true
        }
    }
}
