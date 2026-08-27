// SPDX-License-Identifier: MIT
// Copyright (C) 2026 Frank Secilia

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    spacing: 6

    readonly property bool editingEnabled: app.dpiConfigured

    RowLayout {
        Layout.fillWidth: true
        spacing: 16

        GroupBox {
            title: qsTr("Device")
            Layout.fillWidth: true

            ColumnLayout {
                anchors.fill: parent
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 16

                    ComboBox {
                        Layout.minimumWidth: 18*em
                        enabled: root.editingEnabled
                        model: sessionView.attachmentLabels
                        currentIndex: sessionView.selectedAttachmentIndex
                        onActivated: (index) => sessionView.selectAttachment(index)
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 24

                        property var sectionModel: deviceModel
                        property int layoutMode: PropertyDelegateChooser.Mode.Horizontal
                        property bool controlsEnabled: root.editingEnabled
                        property string enabledPath: "dpi"
                        property string focusPath: root.editingEnabled ? "" : "dpi"

                        Repeater {
                            model: deviceModel
                            delegate: PropertyDelegateChooser {}
                        }
                    }
                }

                Button {
                    text: qsTr("Refresh")
                    enabled: root.editingEnabled
                    onClicked: sessionView.refresh()
                }
            }
        }

        GroupBox {
            title: qsTr("Profile")
            enabled: root.editingEnabled
            Layout.fillWidth: true

            RowLayout {
                anchors.fill: parent
                spacing: 24

                property var sectionModel: profileModel
                property int layoutMode: PropertyDelegateChooser.Mode.Horizontal
                Repeater {
                    model: profileModel
                    delegate: PropertyDelegateChooser {}
                }
            }
        }

        RowLayout {
            enabled: root.editingEnabled && sessionView.canApply
            Layout.alignment: Qt.AlignBottom
            Layout.bottomMargin: 9

            Button {
                text: qsTr("Apply")
                onClicked: app.apply()
            }

            Button {
                text: qsTr("Disable")
                onClicked: app.disable()
            }
        }
    }

    Label {
        text: sessionView.statusText
        visible: text !== ""
        color: sessionView.statusIsError ? "red" : sysPalette.text
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }
}
