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
                text: qsTr("Appearance")
                color: Theme.text
                font.pixelSize: 17
                font.bold: true
            }

            RowLayout {
                Layout.fillWidth: true

                ColumnLayout {
                    spacing: 2
                    Layout.fillWidth: true

                    Label {
                        text: qsTr("Dark theme")
                        color: Theme.text
                        font.pixelSize: 15
                    }

                    Label {
                        text: qsTr("Switch between the dark and light look. The choice is remembered.")
                        color: Theme.textMuted
                        font.pixelSize: 13
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }

                Switch {
                    checked: app.theme !== "light"
                    onToggled: app.theme = checked ? "dark" : "light"
                }
            }

            RowLayout {
                Layout.fillWidth: true

                ColumnLayout {
                    spacing: 2
                    Layout.fillWidth: true

                    Label {
                        text: qsTr("Accent color")
                        color: Theme.text
                        font.pixelSize: 15
                    }

                    Label {
                        text: qsTr("Tints buttons, highlights and the active tab. The choice is remembered.")
                        color: Theme.textMuted
                        font.pixelSize: 13
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }

                Row {
                    spacing: 10

                    Repeater {
                        model: ["#2d7ff9", "#7c5cff", "#e0517d", "#ee8034", "#26a269"]

                        delegate: Rectangle {
                            required property string modelData
                            width: 26
                            height: 26
                            radius: 13
                            color: modelData
                            border.width: app.accentColor === modelData ? 2 : 1
                            border.color: app.accentColor === modelData
                                          ? Theme.text : Theme.border

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: app.accentColor = parent.modelData
                            }
                        }
                    }
                }
            }

            Label {
                text: qsTr("Connection")
                color: Theme.text
                font.pixelSize: 17
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
                    font.pixelSize: 14
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
                    font.pixelSize: 14
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
                font.pixelSize: 17
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
                font.pixelSize: 13
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            Label {
                text: qsTr("Storage")
                color: Theme.text
                font.pixelSize: 17
                font.bold: true
            }

            Label {
                text: qsTr("Database") + ": " + app.databasePath
                color: Theme.textMuted
                font.pixelSize: 13
                font.family: Theme.mono
                wrapMode: Text.ElideMiddle
                Layout.fillWidth: true
            }

            Label {
                text: qsTr("Updates")
                color: Theme.text
                font.pixelSize: 17
                font.bold: true
            }

            RowLayout {
                Layout.fillWidth: true

                ColumnLayout {
                    spacing: 2
                    Layout.fillWidth: true

                    Label {
                        text: qsTr("Auto check update")
                        color: Theme.text
                        font.pixelSize: 15
                    }

                    Label {
                        text: qsTr("Look for a newer release when the app starts and show a notice when one is found.")
                        color: Theme.textMuted
                        font.pixelSize: 13
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }

                Switch {
                    checked: app.autoCheckUpdates
                    onToggled: app.autoCheckUpdates = checked
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Button {
                    text: app.checkingUpdate ? qsTr("Checking…") : qsTr("Check for updates")
                    font.pixelSize: 14
                    enabled: !app.checkingUpdate
                    onClicked: app.checkForUpdates()
                }

                Button {
                    visible: app.updateAvailable
                    highlighted: true
                    text: qsTr("Download %1").arg(app.latestVersion)
                    font.pixelSize: 14
                    onClicked: app.openReleasePage()
                }

                Item { Layout.fillWidth: true }

                Label {
                    visible: !app.checkingUpdate && app.latestVersion.length > 0
                             && !app.updateAvailable
                    text: qsTr("Up to date — %1 is the latest release").arg(app.latestVersion)
                    color: Theme.textMuted
                    font.pixelSize: 13
                }
            }

            Label {
                text: qsTr("About")
                color: Theme.text
                font.pixelSize: 17
                font.bold: true
            }

            Label {
                text: qsTr("PS5 Package Sender %1 — a portable desktop client that "
                    + "installs PKG packages and copies disc images to a "
                    + "jailbroken PlayStation 5 over the LAN. The console side "
                    + "runs pkg-receiver.elf; grab it from "
                    + "AldySan/ps5-package-receiver-elf on GitHub.").arg(app.version)
                color: Theme.textMuted
                font.pixelSize: 14
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            Item { Layout.fillHeight: true }
        }
    }
}
