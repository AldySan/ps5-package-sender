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

    header: ToolBar {
        height: 52
        background: Rectangle { color: Theme.surface }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            spacing: 12

            Label {
                text: "PS5 Package Sender"
                font.pixelSize: 17
                font.bold: true
                color: Theme.text
            }

            Label {
                text: app.version
                color: Theme.textMuted
                font.pixelSize: 12
            }

            Item { Layout.fillWidth: true }

            Repeater {
                model: ["Library", "Queue", "History", "Settings"]

                delegate: Button {
                    readonly property bool active: pages.currentIndex === index
                    flat: true
                    text: modelData
                    implicitHeight: 34
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

    StackLayout {
        id: pages
        anchors.fill: parent
        currentIndex: 0

        LibraryPage {}
        QueuePage {}
        HistoryPage {}
        SettingsPage {}
    }
}
