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
                    delegate: PropertyDelegateChooser{}
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
                    delegate: PropertyDelegateChooser{}
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
                    delegate: PropertyDelegateChooser{}
                }
            }
        }

        GroupBox {
            title: "Scale"
            Layout.fillWidth: true
            Layout.margins: 8

            ColumnLayout {
                width: parent.width
                property var sectionModel: scaleModel
                Repeater {
                    model: scaleModel
                    delegate: PropertyDelegateChooser{}
                }
            }
        }

        GroupBox {
            title: "Offset"
            Layout.fillWidth: true
            Layout.margins: 8

            ColumnLayout {
                width: parent.width
                property var sectionModel: offsetModel
                Repeater {
                    model: offsetModel
                    delegate: PropertyDelegateChooser{}
                }
            }
        }

        GroupBox {
            title: "Floor"
            Layout.fillWidth: true
            Layout.margins: 8

            ColumnLayout {
                width: parent.width
                property var sectionModel: floorModel
                Repeater {
                    model: floorModel
                    delegate: PropertyDelegateChooser{}
                }
            }
        }

        GroupBox {
            title: "Limit"
            Layout.fillWidth: true
            Layout.margins: 8

            ColumnLayout {
                width: parent.width
                property var sectionModel: limitModel
                Repeater {
                    model: limitModel
                    delegate: PropertyDelegateChooser{}
                }
            }
        }
    }
}