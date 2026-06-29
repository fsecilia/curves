// SPDX-License-Identifier: MIT
// Copyright (C) 2026 Frank Secilia

// Numeric value edit control.

import QtQuick
import QtQuick.Controls

// Abstract numeric text edit control. Adds fields `decimals`, `validator`, and `stepFunction`.
TextField {
    id: inputField
    horizontalAlignment: TextInput.AlignRight

    // set by the concrete field
    property real value: 0.0
    property real min: -99999.999
    property real max:  99999.999
    property int decimals: 0
    property var stepFunction // (value, direction) -> next clamped value

    property string errorMessage: ""
    readonly property bool hasErrorMessage: errorMessage !== ""

    color: hasErrorMessage ? "red" : sysPalette.text
    ToolTip.visible: hasErrorMessage && hovered
    ToolTip.text: errorMessage

    signal commitRequested(real newValue)
    signal wheelRequested(real newValue)

    SystemPalette { id: sysPalette; colorGroup: SystemPalette.Active }

    readonly property string display: Number(value).toLocaleString(Qt.locale(), 'f', decimals)
    text: display

    onValueChanged: if (!activeFocus) text = display
    onDisplayChanged: text = display
    onActiveFocusChanged: if (!activeFocus && !acceptableInput) text = display
    onEditingFinished: commit()

    Component.onCompleted: text = display

    function commit() {
        if (!acceptableInput) return

        let parsed
        try { parsed = Number.fromLocaleString(Qt.locale(), text) }
        catch (e) { text = display; return }

        let clamped = Math.min(Math.max(parsed, min), max)
        commitRequested(clamped)
    }

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
            wheelRequested(inputField.stepFunction(base, direction))
            wheel.accepted = true
        }
    }
}
