// SPDX-License-Identifier: MIT
// Copyright (C) 2026 Frank Secilia

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.qmlmodels

ApplicationWindow {
    width: 800
    height: 600
    visible: true
    title: "Curves Configuration"

    // map standard keys to the QUndoStack exposed from c++
    Shortcut {
        sequence: [StandardKey.Undo]
        onActivated: undoStack.undo()
    }
    Shortcut {
        sequence: [StandardKey.Redo]
        onActivated: undoStack.redo()
    }

    SystemPalette {
        id: sysPalette
        colorGroup: SystemPalette.Active
    }

    ListView {
        id: propertyList
        anchors.fill: parent
        clip: true
        model: curvePropertyModel
        spacing: 12
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: ScrollBar {
            id: vbar
            policy: ScrollBar.AsNeeded
            visible: size < 1.0

            contentItem: Rectangle {
                implicitWidth: 16
                radius: 0
                color: sysPalette.text
                opacity: vbar.pressed ? 0.5 : (vbar.hovered ? 0.25 : 0.125)
            }

            background: Item {}
        }

        delegate: DelegateChooser {
            role: "typeId"

            DelegateChoice {
                roleValue: 0 // property_type_id_t::float_t
                delegate: floatDelegate
            }

            DelegateChoice {
                roleValue: 1 // property_type_id_t::int_t
                delegate: intDelegate
            }

            DelegateChoice {
                roleValue: 2 // property_type_id_t::bool_t
                delegate: boolDelegate
            }
        }
    }

    Component {
        id: floatDelegate
        Control {
            id: floatControl
            width: ListView.view.width
            leftPadding: 24
            rightPadding: 24

            function stepLogarithmic(val, direction) {
                let absVal = Math.abs(val)
                let step = 0.01

                if (absVal >= 0.01) {
                    let magnitude = Math.floor(Math.log10(absVal))
                    step = Math.pow(10, magnitude - 1)

                    if (direction < 0 && Math.abs(Math.pow(10, magnitude) - absVal) < 0.000001) {
                        step = Math.pow(10, magnitude - 2)
                    }
                }

                let nextVal = val + (step * direction)
                let clamped = Math.min(Math.max(nextVal, model.minVal !== undefined ? model.minVal : -999999.0),
                                                model.maxVal !== undefined ? model.maxVal : 999999.0)

                return Number(clamped.toFixed(3))
            }

            contentItem: RowLayout {
                spacing: 16
                Label {
                    text: model.path
                    Layout.preferredWidth: 300
                    color: sysPalette.windowText
                }

                TextField {
                    id: inputField
                    Layout.fillWidth: true
                    Layout.minimumWidth: 50
                    horizontalAlignment: TextInput.AlignRight
                    color: sysPalette.text

                    readonly property real low: model.minVal !== undefined ? model.minVal : -999999.0
                    readonly property real high: model.maxVal !== undefined ? model.maxVal :  999999.0
                    readonly property string display: Number(model.value).toLocaleString(Qt.locale(), 'f', 3)

                    text: display
                    validator: DoubleValidator { bottom: inputField.low; top: inputField.high }

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
                        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                            commit()
                            event.accepted = true
                        }
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
                            model.value = floatControl.stepLogarithmic(base, direction)
                            wheel.accepted = true
                        }
                    }
                }
            }
        }
    }

    Component {
        id: intDelegate
        Control {
            width: ListView.view.width
            leftPadding: 24
            rightPadding: 24

            contentItem: RowLayout {
                spacing: 16
                Label {
                    text: model.path
                    Layout.preferredWidth: 300
                    color: sysPalette.windowText
                }

                TextField {
                    id: inputField
                    Layout.fillWidth: true
                    Layout.minimumWidth: 50
                    horizontalAlignment: TextInput.AlignRight
                    color: sysPalette.text

                    readonly property real low: model.minVal !== undefined ? model.minVal : -999999
                    readonly property real high: model.maxVal !== undefined ? model.maxVal :  999999
                    readonly property string display: Number(model.value).toLocaleString(Qt.locale(), 'f', 0)

                    text: display
                    validator: DoubleValidator { bottom: inputField.low; top: inputField.high }

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
                        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                            commit()
                            event.accepted = true
                        }
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
                            let nextVal = Math.round(base) + direction
                            let clamped = Math.min(Math.max(nextVal,
                                model.minVal !== undefined ? model.minVal : -999999),
                                model.maxVal !== undefined ? model.maxVal : 999999)
                            model.value = clamped
                            wheel.accepted = true
                        }
                    }
                }
            }
        }
    }

    Component {
        id: boolDelegate
        Control {
            width: ListView.view.width
            leftPadding: 24
            rightPadding: 24

            contentItem: RowLayout {
                spacing: 16
                Label {
                    text: model.path
                    Layout.preferredWidth: 300
                    color: sysPalette.windowText
                }

                CheckBox {
                    id: boolBox
                    readonly property bool modelChecked: model.value
                    checked: modelChecked
                    onModelCheckedChanged: checked = modelChecked
                    onClicked: model.value = checked
                }

                Item { Layout.fillWidth: true }
            }
        }
    }
}
