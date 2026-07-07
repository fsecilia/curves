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
        //RowLayout {
        //    Layout.fillWidth: true
        //    Layout.fillHeight: true

            // Top Control Bar
            RowLayout {
                Layout.fillWidth: true

                Label {
                    text: "Mouse DPI:"
                    color: sysPalette.text
                }

                SpinBox {
                    id: dpiInput
                    from: 0
                    to: 64000
                    stepSize: 100
                    value: 0
                    editable: true
                }

                Item { Layout.fillWidth: true } // Spacer
            }

            // Graph Area
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: sysPalette.window
                border.color: sysPalette.mid

                GraphWidget {
                    id: graph
                    anchors.fill: parent
                    anchors.margins: 1

                    dpi: dpiInput.value
                    evaluator: app.evaluator

                    DragHandler {
                        target: null
                        enabled: graph.dpi > 0
                        onTranslationChanged: (delta) => {
                            let domainDx = -(delta.x / graph.width) * graph.domain.width
                            let domainDy = (delta.y / graph.height) * graph.domain.height

                            graph.domain = Qt.rect(
                                graph.domain.x + domainDx,
                                graph.domain.y + domainDy,
                                graph.domain.width,
                                graph.domain.height
                            )
                        }
                    }

                    WheelHandler {
                        target: null
                        property: "rotation"
                        enabled: graph.dpi > 0
                        onWheel: (event) => {
                            let zoomFactor = event.angleDelta.y > 0 ? 0.9 : 1.1;
                            let newWidth = graph.domain.width * zoomFactor
                            let newHeight = graph.domain.height * zoomFactor

                            let cx = graph.domain.x + graph.domain.width / 2.0
                            let cy = graph.domain.y + graph.domain.height / 2.0

                            graph.domain = Qt.rect(
                                cx - newWidth / 2.0,
                                cy - newHeight / 2.0,
                                newWidth,
                                newHeight
                            )
                        }
                    }
                }

                Connections {
                    target: app

                    function evaluatorChanged(evaluator) {
                        graph.evaluator = evaluator
                    }
                }
            }
        // }
    }
}