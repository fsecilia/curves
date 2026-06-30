// SPDX-License-Identifier: MIT
// Copyright (C) 2026 Frank Secilia

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.qmlmodels

DelegateChooser {
    role: "typeId"

    enum Mode {
        Vertical,
        Horizontal
    }

    // float control
    DelegateChoice {
        roleValue: 0
        delegate: RowLayout {
            id: floatControl

            readonly property int layoutMode: parent.layoutMode !== undefined
                ? parent.layoutMode
                : PropertyDelegateChooser.Mode.Vertical
            readonly property bool isHorizontal: layoutMode === PropertyDelegateChooser.Mode.Horizontal

            Layout.fillWidth: true
            spacing: 8

            // static label
            Label {
                text: model.path
                Layout.alignment: Qt.AlignRight
                Layout.preferredWidth: floatControl.isHorizontal ? implicitWidth : 100
                Layout.fillWidth: false
            }

            // numeric text box
            Loader {
                source: qtVersion >= 0x060900 ? "FloatField_v6_9.qml" : "FloatField.qml"

                Layout.fillWidth: true
                Layout.minimumWidth: 50

                onLoaded: {
                    item.value = Qt.binding(() => model.value)
                    item.min = Qt.binding(() => model.min !== undefined ? model.min : -999999.0)
                    item.max = Qt.binding(() => model.max !== undefined ? model.max : 999999.0)

                    item.errorMessage = Qt.binding(() => model.errorMessage !== undefined ? model.errorMessage : "")

                    item.onCommitRequested.connect((val) => { model.value = val })
                    item.onWheelRequested.connect((val) =>
                    {
                        floatControl.parent.sectionModel.on_wheel(index, val)
                    })
                }
            }
        }
    }

    // int control
    DelegateChoice {
        roleValue: 1
        delegate: RowLayout {
            id: intControl

            readonly property int layoutMode: parent.layoutMode !== undefined
                ? parent.layoutMode
                : PropertyDelegateChooser.Mode.Vertical
            readonly property bool isHorizontal: layoutMode === PropertyDelegateChooser.Mode.Horizontal

            Layout.fillWidth: true
            spacing: 16

            // static label
            Label {
                text: model.path
                Layout.alignment: Qt.AlignRight
                Layout.preferredWidth: intControl.isHorizontal ? implicitWidth : 100
                Layout.fillWidth: false
            }

            // numeric text box
            Loader {
                source: qtVersion >= 0x060900 ? "IntField_v6_9.qml" : "IntField.qml"

                Layout.fillWidth: true
                Layout.minimumWidth: 50

                onLoaded: {
                    item.value = Qt.binding(() => model.value)
                    item.min = Qt.binding(() => model.min !== undefined ? model.min : -999999)
                    item.max = Qt.binding(() => model.max !== undefined ? model.max : 999999)

                    item.errorMessage = Qt.binding(() => model.errorMessage !== undefined ? model.errorMessage : "")

                    item.onCommitRequested.connect((val) => { model.value = val })
                    item.onWheelRequested.connect((val) =>
                    {
                        intControl.parent.sectionModel.on_wheel(index, val)
                    })
                }
            }
        }
    }

    // bool control
    DelegateChoice {
        roleValue: 2
        delegate: RowLayout {
            id: boolControl
            Layout.preferredWidth: 100
            spacing: 8

            Label {
                text: model.path
                Layout.alignment: Qt.AlignRight
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