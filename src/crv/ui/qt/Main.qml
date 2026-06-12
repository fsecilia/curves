// SPDX-License-Identifier: MIT
// Copyright (C) 2026 Frank Secilia

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    width: 1000
    height: 600
    visible: true
    title: "Curves Configuration"

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

    SplitView {
        anchors.fill: parent
        orientation: Qt.Horizontal

        CurveSelector {
            SplitView.preferredWidth: 200
            SplitView.minimumWidth: 150
        }

        CurveConfig {
            SplitView.preferredWidth: 350
            SplitView.minimumWidth: 300
        }

        GraphView {
            SplitView.fillWidth: true
            SplitView.minimumWidth: 300
        }
    }
}