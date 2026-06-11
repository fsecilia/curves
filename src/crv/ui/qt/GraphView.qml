// SPDX-License-Identifier: MIT
// Copyright (C) 2026 Frank Secilia

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    color: sysPalette.base

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 16

        // placeholder for the graph
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: sysPalette.window
            border.color: sysPalette.mid

            Label {
                anchors.centerIn: parent
                text: "Graph Rendering Area\n(wip)"
                horizontalAlignment: Text.AlignHCenter
                color: sysPalette.text
            }
        }

        // bottom bar
        RowLayout {
            Layout.fillWidth: true

            Label {
                id: errorLabel
                Layout.fillWidth: true
                color: "red"
                text: ""
                wrapMode: Text.WordWrap
            }

            Button {
                text: "Apply"
                onClicked: console.log("apply to device requested")
            }
        }
    }
}