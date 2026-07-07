// SPDX-License-Identifier: MIT
// Copyright (C) 2026 Frank Secilia

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    Layout.fillWidth: true
    Layout.fillHeight: true

    color: sysPalette.window
    border.color: sysPalette.mid

    GraphWidget {
        id: graph
        anchors.fill: parent
        anchors.margins: 1

        dpi: deviceModel.get_value("dpi")
        evaluator: app.evaluator

        DragHandler {
            target: null
            enabled: graph.dpi > 0
            onTranslationChanged: (delta) => {
                graph.pan(delta)
            }
        }

        WheelHandler {
            target: null
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            enabled: graph.dpi > 0

            onWheel: (event) => {
                graph.zoom(event.angleDelta, point.position)
            }
        }
    }

    Connections {
        target: deviceModel

        function onValueChanged(path, value) {
            if (path == "dpi") graph.dpi = value
        }
    }
}
