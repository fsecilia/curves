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

                    text: Number(model.value).toLocaleString(Qt.locale(), 'f', 3)

                    validator: DoubleValidator {
                        bottom: model.minVal !== undefined ? model.minVal : -999999.0
                        top: model.maxVal !== undefined ? model.maxVal : 999999.0
                    }

                    property bool isEditingLocally: false

                    // clear state if user clicks away or list recycles delegate
                    onActiveFocusChanged: {
                        if (!activeFocus) {
                            isEditingLocally = false
                        }
                    }

                    onTextEdited: {
                        isEditingLocally = true
                    }

                    onEditingFinished: {
                        let parsed
                        try { parsed = Number.fromLocaleString(Qt.locale(), text) }
                        catch (e) {
                            text = Qt.binding(() => Number(model.value).toLocaleString(Qt.locale(), 'f', 3))
                            return
                        }

                        let clamped = Math.min(Math.max(parsed,
                            model.minVal !== undefined ? model.minVal : -999999.0),
                            model.maxVal !== undefined ? model.maxVal : 999999.0)
                        isEditingLocally = false
                        model.value = clamped
                        text = Qt.binding(() => Number(model.value).toLocaleString(Qt.locale(), 'f', 3))
                    }

                    property var externalModelValue: model.value

                    onExternalModelValueChanged: {
                        if (isEditingLocally) {
                            // user dragged graph or hit global undo
                            isEditingLocally = false
                            inputField.focus = false
                            text = Qt.binding(() => Number(model.value).toLocaleString(Qt.locale(), 'f', 3))
                        }
                    }

                    Keys.onPressed: (event) => {
                        if (event.matches(StandardKey.Undo)) {
                            if (isEditingLocally && inputField.canUndo) {
                                // let local text engine handle undo
                                event.accepted = false
                            } else {
                                // input is committed; swallow and use real undo
                                undoStack.undo()
                                event.accepted = true
                            }
                        }
                        else if (event.matches(StandardKey.Redo)) {
                            if (isEditingLocally && inputField.canRedo) {
                                event.accepted = false
                            } else {
                                undoStack.redo()
                                event.accepted = true
                            }
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.NoButton

                        onWheel: (wheel) => {
                            // if user hasn't clicked into this specific box, let ListView scroll
                            if (!inputField.activeFocus) {
                                wheel.accepted = false
                                return
                            }

                            let direction = wheel.angleDelta.y > 0 ? 1 : -1
                            model.value = floatControl.stepLogarithmic(Number(model.value), direction)
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

                    text: Number(model.value).toLocaleString(Qt.locale(), 'f', 0)
                    validator: IntValidator {
                        bottom: model.minVal !== undefined ? model.minVal : -999999
                        top: model.maxVal !== undefined ? model.maxVal : 999999
                    }

                    property bool isEditingLocally: false

                    // clear state if user clicks away or list recycles delegate
                    onActiveFocusChanged: {
                        if (!activeFocus) {
                            isEditingLocally = false
                        }
                    }

                    onTextEdited: {
                        isEditingLocally = true
                    }

                    onEditingFinished: {
                        let parsed
                        try { parsed = Number.fromLocaleString(Qt.locale(), text) }
                        catch (e) {
                            text = Qt.binding(() => Number(model.value).toLocaleString(Qt.locale(), 'f', 0))
                            return
                        }

                        let clamped = Math.min(Math.max(parsed,
                            model.minVal !== undefined ? model.minVal : -999999),
                            model.maxVal !== undefined ? model.maxVal : 999999)
                        isEditingLocally = false
                        model.value = clamped
                        text = Qt.binding(() => Number(model.value).toLocaleString(Qt.locale(), 'f', 0))
                    }

                    property var externalModelValue: model.value

                    onExternalModelValueChanged: {
                        if (isEditingLocally) {
                            // user dragged graph or hit global undo
                            isEditingLocally = false
                            inputField.focus = false
                            text = Qt.binding(() => Number(model.value).toLocaleString(Qt.locale(), 'f', 0))
                        }
                    }

                    Keys.onPressed: (event) => {
                        if (event.matches(StandardKey.Undo)) {
                            if (isEditingLocally && inputField.canUndo) {
                                // let local text engine handle undo
                                event.accepted = false
                            } else {
                                // input is committed; swallow and use real undo
                                undoStack.undo()
                                event.accepted = true
                            }
                        }
                        else if (event.matches(StandardKey.Redo)) {
                            if (isEditingLocally && inputField.canRedo) {
                                event.accepted = false
                            } else {
                                undoStack.redo()
                                event.accepted = true
                            }
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.NoButton

                        onWheel: (wheel) => {
                            // if user hasn't clicked into this specific box, let ListView scroll
                            if (!inputField.activeFocus) {
                                wheel.accepted = false
                                return
                            }

                            let direction = wheel.angleDelta.y > 0 ? 1 : -1
                            let step = 1
                            let nextVal = model.value + (step * direction)

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
                    checked: model.value

                    onClicked: {
                        model.value = checked
                        checked = Qt.binding(() => model.value)
                    }
                }

                Item { Layout.fillWidth: true } // spacer to keep the checkbox aligned left next to the label
            }
        }
    }
}
