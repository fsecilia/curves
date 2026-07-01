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

    ScrollBar.vertical: ScrollBar {
        id: scrollBar
        policy: scrollView.contentHeight > scrollView.height ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
    }

    Control {
        leftPadding: 2
        width: parent ? Math.max(0, parent.width - leftPadding) : 0
        Layout.fillWidth: true

        ColumnLayout {
            id: mainLayout
            width: parent.width

            GroupBox {
                title: qsTr("Scale")
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
                title: qsTr("Offset")
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
                title: qsTr("Baseline")
                Layout.fillWidth: true
                Layout.margins: 8

                ColumnLayout {
                    width: parent.width
                    property var sectionModel: baselineModel
                    Repeater {
                        model: baselineModel
                        delegate: PropertyDelegateChooser{}
                    }
                }
            }

            GroupBox {
                title: qsTr("Limit")
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

            GroupBox {
                title: qsTr("Active Curve")
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
        }
    }
}