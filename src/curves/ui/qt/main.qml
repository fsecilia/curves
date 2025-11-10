import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

// Import all the C++ types we registered in main.cpp
import dev.mouse.curves 1.0

Window {
    id: root
    width: 1024
    height: 600
    visible: true
    title: "Mouse Acceleration Config"
    color: "#343434"

    // This property is a *default* value.
    // Your C++ 'EditorPresenter' will calculate the real
    // width and expose it.
    // We bind to the 'editorPresenter' instance from main.cpp
    property int curveListWidth: editorPresenter.curveListWidth

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // --- Section 1: Curve List (TabBar) ---
        ScrollView {
            id: tabScrollView
            Layout.fillHeight: true
            Layout.preferredWidth: curveListWidth
            Layout.minimumWidth: 100
            Layout.maximumWidth: 250
            clip: true
            background: Rectangle { color: "#2E2E2E" }

            ListView {
                id: curveList
                width: parent.width
                model: editorPresenter.curveTypeModel

                focus: true
                currentIndex: 0  // Default selection
                onCurrentIndexChanged: {
                    editorPresenter.onCurveTypeSelected(currentIndex)
                    forceActiveFocus()
                }

                Component.onCompleted: {
                    forceActiveFocus()  // Explicitly grab keyboard focus
                }

                delegate: ItemDelegate {
                    width: ListView.view.width
                    text: model.displayName
                    highlighted: ListView.isCurrentItem

                    onClicked: {
                        curveList.currentIndex = index
                        editorPresenter.onCurveTypeSelected(index)
                    }

                    // Style like your tabs
                    palette.text: highlighted ? "white" : "#BBBBBB"
                    background: Rectangle {
                        color: highlighted ? "#007ACC" : "transparent"
                    }
                }
            }
        }

        // --- Section 2: Parameter List (ListView) ---
        Rectangle {
            Layout.preferredWidth: 200
            Layout.fillHeight: true
            color: "#3A3A3A"

            ListView {
                id: parameterList
                anchors.fill: parent
                clip: true

                // Bind to the second model from the presenter
                model: editorPresenter.curveParameterModel

                // Use our custom QML component for each row
                delegate: ParameterDelegate {
                    // 'model' is automatically provided here
                }
            }
        }

        // --- Section 3: View & Footer ---
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // Use our registered C++ component
            QtView {
                id: view
                Layout.fillWidth: true
                Layout.fillHeight: true
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 50
                color: "#2E2E2E"

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 10
                    Button { text: "Apply" }
                    Button { text: "Reset" }
                    Item { Layout.fillWidth: true }
                }
            }
        }
    }
}
