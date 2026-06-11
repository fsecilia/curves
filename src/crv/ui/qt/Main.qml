// SPDX-License-Identifier: MIT
// Copyright (C) 2026 Frank Secilia

// Main window.

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
        model: curveModel
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
            contentItem: RowLayout {
                spacing: 16
                Label {
                    text: model.path
                    Layout.preferredWidth: 300
                    color: sysPalette.windowText
                }
                Loader {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 50
                    source: qtVersion >= 0x060900 ? "FloatField_v6_9.qml" : "FloatField.qml"
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
                Loader {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 50
                    source: qtVersion >= 0x060900 ? "IntField_v6_9.qml" : "IntField.qml"
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
