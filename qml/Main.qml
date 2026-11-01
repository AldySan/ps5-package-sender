import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import PS5PackageSender

ApplicationWindow {
    id: root
    width: 1180
    height: 760
    minimumWidth: 900
    minimumHeight: 600
    title: app.title
    color: Theme.background
    visible: true

    header: ColumnLayout {
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 46
            color: Theme.surface

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 12

                Label {
                    text: "PS5 Package Sender"
                    font.pixelSize: 16
                    font.bold: true
                    color: Theme.text
                }

                Label {
                    text: app.version
                    color: Theme.textMuted
                    font.pixelSize: 12
                }

                Item { Layout.fillWidth: true }

                Label {
                    visible: app.scanning
                    text: app.scanStatus
                    color: Theme.textMuted
                    font.pixelSize: 11
                }

                Repeater {
                    model: ["Library", "Queue", "History", "Settings"]

                    delegate: Button {
                        readonly property bool active: pages.currentIndex === index
                        flat: true
                        text: qsTr(modelData)
                        implicitHeight: 32
                        font.pixelSize: 13
                        font.bold: active
                        onClicked: pages.currentIndex = index

                        background: Rectangle {
                            radius: Theme.radius
                            color: active ? Theme.accentSoft : "transparent"
                            border.width: active ? 1 : 0
                            border.color: Theme.accent
                        }

                        contentItem: Label {
                            text: parent.text
                            color: parent.active ? Theme.accent : Theme.textMuted
                            font: parent.font
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }
            }
        }

        StatusBar {
            Layout.fillWidth: true
        }
    }

    StackLayout {
        id: pages
        anchors.fill: parent
        currentIndex: 0

        LibraryPage {}
        QueuePage {}
        HistoryPage {}
        SettingsPage {}
    }

    DropArea {
        anchors.fill: parent
        onDropped: drop => {
            const urls = []
            for (let i = 0; i < drop.urls.length; i++)
                urls.push(drop.urls[i])
            app.addDroppedUrls(urls)
            pages.currentIndex = 0
        }
        Rectangle {
            anchors.fill: parent
            color: Theme.accentSoft
            border.width: 2
            border.color: Theme.accent
            radius: Theme.radius
            visible: parent.containsDrag
            Label {
                anchors.centerIn: parent
                text: qsTr("Drop PKG files or folders")
                color: Theme.accent
                font.pixelSize: 16
            }
        }
    }

    Toast {
        id: toast
    }

    Connections {
        target: app
        function onNotify(message) {
            toast.show(message)
        }
    }
}
