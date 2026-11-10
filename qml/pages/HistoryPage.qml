import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import PS5PackageSender

Page {
    id: root
    background: Rectangle { color: Theme.background }

    onVisibleChanged: if (visible) app.reloadHistory()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 10

        RowLayout {
            spacing: 10

            Label {
                text: app.historyRows.length + qsTr(" record(s)")
                color: Theme.text
                font.pixelSize: 13
                font.bold: true
            }

            Item { Layout.fillWidth: true }

            AppButton {
                text: qsTr("Clear history")
                enabled: app.historyRows.length > 0
                onClicked: app.clearHistory()
            }
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 6
            model: app.historyRows
            ScrollBar.vertical: ScrollBar {}

            delegate: Rectangle {
                width: list.width
                height: 56
                radius: Theme.radius
                color: Theme.surface

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 10

                    Rectangle {
                        width: kindLabel.implicitWidth + 10
                        height: kindLabel.implicitHeight + 4
                        radius: 4
                        color: modelData.kind === "copy" ? "#22f5b83d" : Theme.accentSoft

                        Label {
                            id: kindLabel
                            anchors.centerIn: parent
                            text: modelData.kind === "copy" ? qsTr("COPY") : qsTr("INSTALL")
                            color: modelData.kind === "copy" ? Theme.warn : Theme.accentText
                            font.pixelSize: 9
                            font.bold: true
                        }
                    }

                    ColumnLayout {
                        spacing: 2
                        Layout.fillWidth: true

                        Label {
                            text: modelData.name
                            color: Theme.text
                            font.pixelSize: 13
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }

                        Label {
                            visible: modelData.remotePath.length > 0
                            text: modelData.remotePath
                            color: Theme.textMuted
                            font.pixelSize: 10
                            font.family: Theme.mono
                            elide: Text.ElideMiddle
                            Layout.fillWidth: true
                        }
                    }

                    ColumnLayout {
                        spacing: 2

                        Label {
                            text: modelData.sizeText
                            color: Theme.textMuted
                            font.pixelSize: 11
                            Layout.alignment: Qt.AlignRight
                        }

                        Label {
                            text: modelData.ok ? qsTr("ok") : qsTr("failed")
                            color: modelData.ok ? Theme.ok : Theme.error
                            font.pixelSize: 11
                            Layout.alignment: Qt.AlignRight
                        }
                    }

                    Label {
                        text: modelData.time
                        color: Theme.textMuted
                        font.pixelSize: 11
                    }
                }
            }
        }

        Label {
            visible: app.historyRows.length === 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            text: qsTr("Finished transfers will be recorded here")
            color: Theme.textMuted
            font.pixelSize: 14
        }
    }
}
