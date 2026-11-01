import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import PS5PackageSender

Page {
    id: root
    background: Rectangle { color: Theme.background }

    Flickable {
        anchors.fill: parent
        anchors.margins: 16
        contentHeight: content.implicitHeight
        clip: true
        ScrollBar.vertical: ScrollBar {}

        ColumnLayout {
            id: content
            width: Math.min(680, root.width - 32)
            spacing: 18

            Label {
                text: qsTr("Connection")
                color: Theme.text
                font.pixelSize: 15
                font.bold: true
            }

            GridLayout {
                columns: 2
                rowSpacing: 10
                columnSpacing: 10
                Layout.fillWidth: true

                Label {
                    text: qsTr("Console IP")
                    color: Theme.textMuted
                    font.pixelSize: 12
                }

                TextField {
                    selectByMouse: true
                    placeholderText: qsTr("192.168.1.42")
                    text: app.psIp
                    onEditingFinished: app.psIp = text.trim()
                    Layout.fillWidth: true
                }

                Label {
                    text: qsTr("PC address")
                    color: Theme.textMuted
                    font.pixelSize: 12
                }

                ComboBox {
                    model: app.pcAddresses
                    currentIndex: app.pcIp ? Math.max(0, app.pcAddresses.indexOf(app.pcIp)) : -1
                    displayText: app.pcIp || qsTr("no network")
                    onActivated: index => app.pcIp = app.pcAddresses[index]
                    Layout.fillWidth: true
                }
            }

            Label {
                text: qsTr("Scan paths")
                color: Theme.text
                font.pixelSize: 15
                font.bold: true
            }

            DestinationPicker {
                Layout.fillWidth: true
            }

            Label {
                text: qsTr("Copies land directly in a folder the console mounter "
                    + "scans, so dropped images are picked up on its next scan "
                    + "without extra steps.")
                color: Theme.textMuted
                font.pixelSize: 11
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            Label {
                text: qsTr("Storage")
                color: Theme.text
                font.pixelSize: 15
                font.bold: true
            }

            Label {
                text: qsTr("Database") + ": " + app.databasePath
                color: Theme.textMuted
                font.pixelSize: 11
                font.family: Theme.mono
                wrapMode: Text.ElideMiddle
                Layout.fillWidth: true
            }

            Label {
                text: qsTr("About")
                color: Theme.text
                font.pixelSize: 15
                font.bold: true
            }

            Label {
                text: qsTr("PS5 Package Sender %1 — a portable desktop client that "
                    + "installs PKG packages and copies disc images to a "
                    + "jailbroken PlayStation 5 over the LAN. The console side "
                    + "runs pkg-receiver.elf; grab it from "
                    + "AldySan/ps5-package-receiver-elf on GitHub.").arg(app.version)
                color: Theme.textMuted
                font.pixelSize: 12
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            Item { Layout.fillHeight: true }
        }
    }
}
