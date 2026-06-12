// SPDX-License-Identifier: MIT
// Copyright (C) 2026 Frank Secilia

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.qmlmodels

Flickable {
    id: scrollView
    clip: true
    contentWidth: width
    contentHeight: mainLayout.height
    boundsBehavior: Flickable.StopAtBounds

    ScrollBar.vertical: ScrollBar {}

    ColumnLayout {
        id: mainLayout
        width: parent.width
        spacing: 16

        GroupBox {
            title: "Device"
            Layout.fillWidth: true
            Layout.margins: 8

            ColumnLayout {
                width: parent.width
                property var sectionModel: deviceModel
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
                property var sectionModel: profileModel
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
                property var sectionModel: specificCurveModel
                Repeater {
                    model: specificCurveModel
                    delegate: propertyChooser
                }
            }
        }

        GroupBox {
            title: "Scale"
            Layout.fillWidth: true; Layout.margins: 8
            ColumnLayout {
                width: parent.width
                property var sectionModel: scaleModel
                Repeater {
                    model: scaleModel
                    delegate: propertyChooser
                }
            }
        }

        GroupBox {
            title: "Offset"
            Layout.fillWidth: true; Layout.margins: 8
            ColumnLayout {
                width: parent.width
                property var sectionModel: offsetModel
                Repeater {
                    model: offsetModel
                    delegate: propertyChooser
                }
            }
        }

        GroupBox {
            title: "Floor"
            Layout.fillWidth: true; Layout.margins: 8
            ColumnLayout {
                width: parent.width
                property var sectionModel: floorModel
                Repeater {
                    model: floorModel
                    delegate: propertyChooser
                }
            }
        }

        GroupBox {
            title: "Limit"
            Layout.fillWidth: true; Layout.margins: 8
            ColumnLayout {
                width: parent.width
                property var sectionModel: limitModel
                Repeater {
                    model: limitModel
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

                    onLoaded: item.sectionModel = floatControl.parent.sectionModel
                }
            }
        }
    }

    Component {
        id: intDelegate
        Control {
            id: intControl
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

                    onLoaded: item.sectionModel = intControl.parent.sectionModel
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