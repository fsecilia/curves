// SPDX-License-Identifier: MIT
// Copyright (C) 2026 Frank Secilia

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    title: "Curves Configuration"
    visible: true

    FontMetrics {
        id: sysFontMetrics
        font: Qt.application.font
    }
    readonly property real em: sysFontMetrics.averageCharacterWidth
    readonly property real fontHeight: sysFontMetrics.height

    width: 160*em
    height: 50*fontHeight
    minimumWidth: 80*em
    minimumHeight: 25*fontHeight

    Action {
        shortcut: StandardKey.Undo
        onTriggered: undoStack.undo()
    }

    Action {
        shortcut: StandardKey.Redo
        onTriggered: undoStack.redo()
    }

    SystemPalette {
        id: sysPalette
        colorGroup: SystemPalette.Active
    }

    ColumnLayout {
        anchors.fill: parent

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true

            CurveSelector {
                Layout.preferredWidth: 15*em
                Layout.maximumWidth: 15*em
                Layout.minimumWidth: 15*em
                Layout.fillHeight: true
            }

            CurveConfig {
                Layout.preferredWidth: 25*em
                Layout.maximumWidth: 25*em
                Layout.minimumWidth: 25*em
                Layout.fillHeight: true
            }

            GraphView {
                Layout.fillWidth: true
                Layout.minimumWidth: 30*em
                Layout.fillHeight: true
            }
        }

        RootConfigRow {
            Layout.fillWidth: true
            Layout.margins: 16
        }
    }
}