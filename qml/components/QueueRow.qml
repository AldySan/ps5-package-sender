import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import PS5PackageSender

Rectangle {
    id: row
    required property var model

    readonly property color stateColor:
        model.state === 4 || model.state === 6 ? Theme.ok
        : model.state === 8 || model.state === 9 ? Theme.error
        : model.state === 5 ? Theme.warn
        : model.state === 7 ? Theme.textMuted
        : Theme.accentText

    color: Theme.surface
    radius: Theme.radius
    implicitHeight: content.implicitHeight + 20

    ColumnLayout {
        id: content
        anchors.fill: parent
        anchors.margins: 10
        spacing: 6

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Image {
                visible: model.iconUrl.length > 0
                source: model.iconUrl
                fillMode: Image.PreserveAspectCrop
                sourceSize.width: 44
                sourceSize.height: 44
            }

            ColumnLayout {
                spacing: 2
                Layout.fillWidth: true

                Label {
                    text: model.name
                    color: Theme.text
                    font.pixelSize: 13
                    font.bold: true
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                RowLayout {
                    spacing: 6

                    Rectangle {
                        width: kindLabel.implicitWidth + 10
                        height: kindLabel.implicitHeight + 4
                        radius: 4
                        color: model.isCopy ? "#22f5b83d" : Theme.accentSoft

                        Label {
                            id: kindLabel
                            anchors.centerIn: parent
                            text: model.isCopy ? qsTr("COPY") : qsTr("INSTALL")
                            color: model.isCopy ? Theme.warn : Theme.accentText
                            font.pixelSize: 11
                            font.bold: true
                        }
                    }

                    Label {
                        text: model.stateText
                        color: row.stateColor
                        font.pixelSize: 11
                        font.bold: true
                    }

                    Label {
                        text: model.sizeText
                        color: Theme.textMuted
                        font.pixelSize: 11
                    }

                    Label {
                        visible: model.message.length > 0
                        text: model.message
                        color: Theme.textMuted
                        font.pixelSize: 11
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }
            }

            ColumnLayout {
                spacing: 2

                Label {
                    text: Math.floor(model.percent) + "%"
                    color: row.stateColor
                    font.pixelSize: 15
                    font.bold: true
                    Layout.alignment: Qt.AlignRight
                }

                RowLayout {
                    spacing: 4

                    IconButton {
                        visible: model.canPause
                        iconName: "pause"
                        ToolTip.text: qsTr("Pause the console copy")
                        onClicked: app.queue.pauseRow(model.id)
                    }

                    IconButton {
                        visible: model.state === 5
                        iconName: "play"
                        ToolTip.text: qsTr("Resume")
                        onClicked: app.queue.resumeRow(model.id)
                    }

                    IconButton {
                        visible: model.canRetry
                        iconName: "refresh"
                        ToolTip.text: qsTr("Retry this item")
                        onClicked: app.queue.retryRow(model.id)
                    }

                    IconButton {
                        visible: model.canCancel
                        iconName: "close"
                        ToolTip.text: qsTr("Cancel")
                        onClicked: app.queue.cancelRow(model.id)
                    }

                    IconButton {
                        iconName: "chevron-up"
                        enabled: model.state === 0
                        ToolTip.text: qsTr("Move up")
                        onClicked: app.queue.moveRow(model.id, -1)
                    }

                    IconButton {
                        iconName: "chevron-down"
                        enabled: model.state === 0
                        ToolTip.text: qsTr("Move down")
                        onClicked: app.queue.moveRow(model.id, 1)
                    }
                }
            }
        }

        ProgressBar {
            Layout.fillWidth: true
            from: 0
            to: 100
            value: model.percent
            background: Rectangle {
                implicitHeight: 6
                radius: 3
                color: Theme.surfaceAlt
            }
            contentItem: Item {
                Rectangle {
                    width: Math.min(parent.width, parent.width * model.percent / 100)
                    height: parent.height
                    radius: 3
                    color: row.stateColor
                }
            }
        }
    }
}
