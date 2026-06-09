// SPDX-License-Identifier: MIT
// Copyright (C) 2026 Frank Secilia

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml.Models

ApplicationWindow {
    width: 800
    height: 600
    visible: true
    title: "Curves Configuration"

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

                    text: Number(model.value).toLocaleString(Qt.locale(), 'f', 4)

                    validator: DoubleValidator {
                        bottom: model.minVal !== undefined ? model.minVal : -999999.0
                        top: model.maxVal !== undefined ? model.maxVal : 999999.0
                    }

                    onEditingFinished: {
                        let parsed = Number.fromLocaleString(Qt.locale(), text)
                        if (!isNaN(parsed)) {
                            let clamped = Math.min(Math.max(parsed, model.minVal !== undefined ? model.minVal : -999999.0),
                                                            model.maxVal !== undefined ? model.maxVal : 999999.0)
                            model.value = clamped
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.NoButton

                        onWheel: (wheel) => {
                            let direction = wheel.angleDelta.y > 0 ? 1 : -1
                            model.value = parent.parent.parent.stepLogarithmic(model.value, direction)
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

                    text: Number(model.value).toLocaleString(Qt.locale(), 'i', 0)
                    validator: IntValidator {
                        bottom: model.minVal !== undefined ? model.minVal : -999999
                        top: model.maxVal !== undefined ? model.maxVal : 999999
                    }

                    onEditingFinished: {
                        let parsed = Number.fromLocaleString(Qt.locale(), text)
                        if (!isNaN(parsed)) {
                            let clamped = Math.min(Math.max(parsed, model.minVal !== undefined ? model.minVal : -999999),
                                                            model.maxVal !== undefined ? model.maxVal : 999999)
                            model.value = clamped
                        }
                    }

                    MouseArea {
                        anchors.fill: parent

                        acceptedButtons: Qt.NoButton

                        onWheel: (wheel) => {
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
                    checked: model.value
                    onClicked: {
                        model.value = checked
                    }
                }

                Item { Layout.fillWidth: true } // spacer to keep the checkbox aligned left next to the label
            }
        }
    }
}
