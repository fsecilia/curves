// SPDX-License-Identifier: MIT
// Copyright (C) 2026 Frank Secilia

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    spacing: 16

    GroupBox {
        title: "Device"
        Layout.fillWidth: true
        ColumnLayout {
            width: parent.width
            property var sectionModel: deviceModel
            Repeater {
                model: deviceModel
                delegate: PropertyDelegateChooser {}
            }
        }
    }

    GroupBox {
        title: "Profile"
        Layout.fillWidth: true
        ColumnLayout {
            width: parent.width
            property var sectionModel: profileModel
            Repeater {
                model: profileModel
                delegate: PropertyDelegateChooser {}
            }
        }
    }

    Button {
        text: "Apply"
        Layout.alignment: Qt.AlignBottom
        Layout.bottomMargin: 8
        onClicked: console.log("apply to device requested")
    }
}