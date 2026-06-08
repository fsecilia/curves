// SPDX-License-Identifier: MIT
// Copyright (C) 2026 Frank Secilia

import QtQuick
import QtQuick.Controls

ApplicationWindow {
    width: 800
    height: 600
    visible: true
    title: "Curves Configuration"

    SystemPalette {
        id: sysPalette
        colorGroup: SystemPalette.Active
    }

    Label {
        anchors.centerIn: parent
        text: "Configuration initialized."
        color: sysPalette.windowText
        font.pixelSize: 24
    }
}