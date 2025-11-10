import QtQuick
import QtQuick.Controls
import QtQuick.Window 

import dev.mouse.curves 1.0

Window {
    id: root
    width: 640
    height: 480
    visible: true
    title: "Mouse Acceleration Config"
    color: "#343434" // Dark background

    // Instantiate our custom C++ curve editor
    QtView {
        id: view
        anchors.fill: parent
        anchors.topMargin: 0
        anchors.bottomMargin: 0 // Place it below the header
        anchors.margins: 20
        anchors.leftMargin: 0
        anchors.rightMargin: 0
    }
}
