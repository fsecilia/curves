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
        RowLayout {
            spacing: 24

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
        RowLayout {
            spacing: 24

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
        Layout.bottomMargin: 9
        onClicked: console.log("apply to device requested")
    }
}