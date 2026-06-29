// SPDX-License-Identifier: MIT
// Copyright (C) 2026 Frank Secilia

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.qmlmodels

DelegateChooser {
    role: "typeId"

    DelegateChoice {
        roleValue: 0
        delegate: Control {
            width: parent ? parent.width : 0
            leftPadding: 24
            rightPadding: 24

            contentItem: GridLayout {
                columns: 2
                columnSpacing: 16
                rowSpacing: 2

                // static label
                Label {
                    text: model.path
                    Layout.preferredWidth: 100
                    Layout.alignment: Qt.AlignRight
                }

                // numeric text box
                Loader {
                    Layout.fillWidth: true
                    source: qtVersion >= 0x060900 ? "FloatField_v6_9.qml" : "FloatField.qml"

                    onLoaded: {
                        item.value = Qt.binding(() => model.value)
                        item.min = Qt.binding(() => model.min !== undefined ? model.min : -999999.0)
                        item.max = Qt.binding(() => model.max !== undefined ? model.max : 999999.0)

                        item.errorMessage = Qt.binding(() => model.errorMessage !== undefined ? model.errorMessage : "")

                        item.onCommitRequested.connect((val) => { model.value = val })
                        item.onWheelRequested.connect((val) => { sectionModel.on_wheel(index, val) })
                    }
                }
            }
        }
    }

    DelegateChoice {
        roleValue: 1
        delegate: Control {
            width: parent ? parent.width : 0
            leftPadding: 24
            rightPadding: 24

            contentItem: GridLayout {
                columns: 2
                columnSpacing: 16
                rowSpacing: 2

                // static label
                Label {
                    text: model.path
                    Layout.preferredWidth: 100
                    Layout.alignment: Qt.AlignRight
                }

                // numeric text box
                Loader {
                    Layout.fillWidth: true
                    source: qtVersion >= 0x060900 ? "IntField_v6_9.qml" : "IntField.qml"

                    onLoaded: {
                        item.value = Qt.binding(() => model.value)
                        item.min = Qt.binding(() => model.min !== undefined ? model.min : -999999)
                        item.max = Qt.binding(() => model.max !== undefined ? model.max : 999999)

                        item.errorMessage = Qt.binding(() => model.errorMessage !== undefined ? model.errorMessage : "")

                        item.onCommitRequested.connect((val) => { model.value = val })
                        item.onWheelRequested.connect((val) => { sectionModel.on_wheel(index, val) })
                    }
                }
            }
        }
    }

    DelegateChoice {
        roleValue: 2
        delegate: Control {
            width: parent ? parent.width : 0
            leftPadding: 24
            rightPadding: 24

            contentItem: RowLayout {
                spacing: 16
                Label {
                    text: model.path
                    Layout.preferredWidth: 100
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