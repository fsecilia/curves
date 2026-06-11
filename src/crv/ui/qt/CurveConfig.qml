// SPDX-License-Identifier: MIT
// Copyright (C) 2026 Frank Secilia

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.qmlmodels

ScrollView {
    id: scrollView
    clip: true
    contentWidth: availableWidth
    // Flickable.boundsBehavior: Flickable.StopAtBounds

    ColumnLayout {
        width: parent.width
        spacing: 16

        GroupBox {
            title: "Device"
            Layout.fillWidth: true
            Layout.margins: 8

            ColumnLayout {
                width: parent.width
                property var activeModel: deviceModel
                Repeater {
                    model: deviceModel
                    delegate: propertyChooser
                }
            }
        }

        GroupBox {
            title: "Profile"
            Layout.fillWidth: true
            Layout.margins: 8

            ColumnLayout {
                width: parent.width
                property var activeModel: profileModel
                Repeater {
                    model: profileModel
                    delegate: propertyChooser
                }
            }
        }

        GroupBox {
            title: "Active Curve"
            Layout.fillWidth: true
            Layout.margins: 8

            ColumnLayout {
                width: parent.width
                property var activeModel: curveModel
                Repeater {
                    model: curveModel
                    delegate: propertyChooser
                }
            }
        }
    }

    DelegateChooser {
        id: propertyChooser
        role: "typeId"
        DelegateChoice { roleValue: 0; delegate: floatDelegate }
        DelegateChoice { roleValue: 1; delegate: intDelegate }
        DelegateChoice { roleValue: 2; delegate: boolDelegate }
    }

    Component {
        id: floatDelegate
        Control {
            id: floatControl
            width: scrollView.width
            leftPadding: 24
            rightPadding: 24
            contentItem: RowLayout {
                spacing: 16
                Label {
                    text: model.path
                    Layout.preferredWidth: 100
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
            width: scrollView.width
            leftPadding: 24
            rightPadding: 24
            contentItem: RowLayout {
                spacing: 16
                Label {
                    text: model.path
                    Layout.preferredWidth: 100
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
            width: scrollView.width
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