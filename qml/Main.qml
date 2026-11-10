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

    palette.window: Theme.surface
    palette.windowText: Theme.text
    palette.base: Theme.field
    palette.text: Theme.text
    palette.button: Theme.surfaceAlt
    palette.buttonText: Theme.text
    palette.highlight: Theme.accent
    palette.highlightedText: "#ffffff"
    palette.mid: Theme.border
    palette.midlight: Theme.border
    palette.light: Theme.border
    palette.dark: Theme.accent
    palette.brightText: "#ffffff"
    palette.shadow: "#000000"
    palette.placeholderText: Theme.textMuted
    palette.link: Theme.link
    palette.linkVisited: Theme.link
    palette.toolTipBase: Theme.toolTipBase
    palette.toolTipText: Theme.toolTipText

    Binding {
        target: Theme
        property: "dark"
        value: app.theme !== "light"
    }

    Binding {
        target: Theme
        property: "accent"
        value: app.accentColor
    }

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
                        id: tabBtn
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
                            border.width: active ? 1 : tabBtn.activeFocus ? 2 : 0
                            border.color: Theme.accent
                        }

                        contentItem: Label {
                            text: parent.text
                            color: parent.active ? Theme.accentText : Theme.textMuted
                            font: parent.font
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }

                Button {
                    id: themeToggle
                    flat: true
                    implicitHeight: 32
                    font.pixelSize: 13
                    text: Theme.dark ? qsTr("☀") : qsTr("☾")
                    ToolTip.visible: hovered
                    ToolTip.text: Theme.dark ? qsTr("Switch to light theme")
                                             : qsTr("Switch to dark theme")
                    onClicked: app.theme = Theme.dark ? "light" : "dark"

                    background: Rectangle {
                        radius: Theme.radius
                        color: Theme.chip
                        border.width: themeToggle.activeFocus ? 2 : 0
                        border.color: Theme.accent
                    }

                    contentItem: Label {
                        text: parent.text
                        color: Theme.text
                        font: parent.font
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
                text: qsTr("Drop PKG/FPKG files or folders")
                color: Theme.accentText
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
