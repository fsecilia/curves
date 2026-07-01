// SPDX-License-Identifier: MIT
// Copyright (C) 2026 Frank Secilia

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    spacing: 16

    GroupBox {
        title: qsTr("Device")
        Layout.fillWidth: true
        RowLayout {
            anchors.fill: parent
            spacing: 24

            property var sectionModel: deviceModel
            property int layoutMode: PropertyDelegateChooser.Mode.Horizontal
            Repeater {
                model: deviceModel
                delegate: PropertyDelegateChooser {}
            }
        }
    }

    GroupBox {
        title: qsTr("Profile")
        Layout.fillWidth: true
        RowLayout {
            anchors.fill: parent
            spacing: 24

            property var sectionModel: profileModel
            property int layoutMode: PropertyDelegateChooser.Mode.Horizontal
            Repeater {
                model: profileModel
                delegate: PropertyDelegateChooser {}
            }
        }
    }

    Button {
        text: qsTr("Apply")
        Layout.alignment: Qt.AlignBottom
        Layout.bottomMargin: 9
        onClicked: console.log("apply to device requested")
    }
}