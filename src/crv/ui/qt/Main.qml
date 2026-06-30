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

        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Horizontal

            CurveSelector {
                SplitView.preferredWidth: 15*em
                SplitView.minimumWidth: 15*em
            }

            CurveConfig {
                SplitView.preferredWidth: 25*em
                SplitView.minimumWidth: 25*em
            }

            GraphView {
                SplitView.fillWidth: true
                SplitView.minimumWidth: 30*em
            }
        }

        RootConfigRow {
            Layout.fillWidth: true
            Layout.margins: 16
        }
    }
}