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
                    id: loader
                    Layout.fillWidth: true
                    source: qtVersion >= 0x060900 ? "FloatField_v6_9.qml" : "FloatField.qml"

                    onLoaded: {
                        item.value = Qt.binding(() => model.value)
                        item.min = Qt.binding(() => model.min !== undefined ? model.min : -999999.0)
                        item.max = Qt.binding(() => model.max !== undefined ? model.max : 999999.0)

                        item.errorMessage = Qt.binding(() => model.errorMessage !== undefined ? model.errorMessage : "")

                        item.onCommitRequested.connect((val) => { model.value = val })
                        item.onWheelRequested.connect((val) => {
                            floatControl.parent.sectionModel.on_wheel(index, val)
                        })
                    }
                }

                // spacer in main label column
                Item { Layout.preferredWidth: 100 }

                // error label
                Label {
                    Layout.fillWidth: true
                    color: "red"
                    font.pointSize: Qt.application.font.pointSize * 0.8

                    text: model.errorSummary !== undefined ? model.errorSummary : ""
                    visible: model.errorSummary

                    bottomPadding: 8
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
                    id: loader
                    Layout.fillWidth: true
                    source: qtVersion >= 0x060900 ? "IntField_v6_9.qml" : "IntField.qml"

                    onLoaded: {
                        item.value = Qt.binding(() => model.value)
                        item.min = Qt.binding(() => model.min !== undefined ? model.min : -999999)
                        item.max = Qt.binding(() => model.max !== undefined ? model.max : 999999)

                        item.errorMessage = Qt.binding(() => model.errorMessage !== undefined ? model.errorMessage : "")

                        item.onCommitRequested.connect((val) => { model.value = val })
                        item.onWheelRequested.connect((val) => {
                            intControl.parent.sectionModel.on_wheel(index, val)
                        })
                    }
                }

                // spacer in main label column
                Item { Layout.preferredWidth: 100 }

                // error label
                Label {
                    Layout.fillWidth: true
                    color: "red"
                    font.pointSize: Qt.application.font.pointSize * 0.8

                    text: model.errorSummary !== undefined ? model.errorSummary : ""
                    visible: model.errorSummary

                    bottomPadding: 8
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