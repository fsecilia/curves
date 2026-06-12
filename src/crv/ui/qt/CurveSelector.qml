// SPDX-License-Identifier: MIT
// Copyright (C) 2026 Frank Secilia

import QtQuick
import QtQuick.Controls

Rectangle {
    color: sysPalette.window

    ListView {
        id: tabList
        anchors.fill: parent
        model: availableCurves
        clip: true

        currentIndex: app.activeCurveIndex

        delegate: ItemDelegate {
            width: ListView.view.width
            text: modelData
            highlighted: ListView.isCurrentItem

            onClicked: app.set_active_curve(index)
        }
    }
}