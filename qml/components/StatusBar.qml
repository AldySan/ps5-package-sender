import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import PS5PackageSender

ToolBar {
    id: root
    height: 56
    background: Rectangle { color: Theme.surface }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        spacing: 10

        Rectangle {
            width: 12
            height: 12
            radius: 6
            color: !app.hasNetwork ? Theme.textMuted
                 : app.receiverOnline ? Theme.ok : Theme.error

            ToolTip.visible: ma.containsMouse
            ToolTip.text: !app.hasNetwork ? qsTr("No network")
                         : app.receiverOnline ? qsTr("Receiver online")
                         : qsTr("Receiver offline")
            MouseArea {
                id: ma
                anchors.fill: parent
                hoverEnabled: true
            }
        }

        Label {
            text: app.receiverOnline
                  ? (app.receiverBusy ? qsTr("console busy") : qsTr("console ready"))
                  : qsTr("no receiver")
            color: app.receiverOnline ? Theme.ok : Theme.error
            font.pixelSize: 12
        }

        Label {
            visible: app.freeSpaceText.length > 0
            text: app.freeSpaceText
            color: Theme.textMuted
            font.pixelSize: 12
        }

        Item { Layout.fillWidth: true }

        ComboBox {
            id: pcBox
            implicitWidth: 150
            model: app.pcAddresses
            visible: app.pcAddresses.length > 0
            currentIndex: app.pcIp ? Math.max(0, app.pcAddresses.indexOf(app.pcIp)) : -1
            displayText: qsTr("PC ") + (app.pcIp || qsTr("IP"))
            onActivated: index => app.pcIp = app.pcAddresses[index]
        }

        TextField {
            id: consoleIpField
            implicitWidth: 130
            placeholderText: qsTr("console IP")
            text: app.psIp
            selectByMouse: true
            onEditingFinished: app.psIp = text.trim()
            font.pixelSize: 12
        }

        Button {
            text: qsTr("Test")
            onClicked: app.testConnection()
        }

        Button {
            text: app.discovering ? qsTr("Detecting…") : qsTr("Detect")
            enabled: !app.discovering
            onClicked: consolePopup.open()
        }
    }

    Popup {
        id: consolePopup
        y: root.height
        width: 360
        height: 420
        padding: 10
        modal: false
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        onOpened: app.detectConsoles()

        background: Rectangle {
            color: Theme.surface
            radius: Theme.radius
            border.width: 1
            border.color: Theme.border
        }

        contentItem: ColumnLayout {
            spacing: 8

            Label {
                text: app.discovering ? app.discoveryStatus : qsTr("Consoles")
                color: Theme.textMuted
                font.pixelSize: 12
                Layout.fillWidth: true
            }

            BusyIndicator {
                visible: app.discovering
                running: app.discovering
                Layout.alignment: Qt.AlignHCenter
            }

            ListView {
                id: consoleList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: app.consoles
                spacing: 6
                ScrollBar.vertical: ScrollBar {}

                delegate: Rectangle {
                    width: consoleList.width
                    height: 52
                    radius: Theme.radius
                    color: modelData.verified ? Theme.accentSoft : Theme.surfaceAlt
                    border.width: modelData.ip === app.psIp ? 1 : 0
                    border.color: Theme.accent

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 2

                        Label {
                            text: modelData.ip
                            color: Theme.text
                            font.pixelSize: 13
                            font.bold: true
                            font.family: Theme.mono
                        }

                        Label {
                            text: modelData.verified
                                  ? (modelData.busy ? qsTr("busy") : qsTr("ready"))
                                  : qsTr("port open, no API reply")
                            color: modelData.verified ? Theme.ok : Theme.warn
                            font.pixelSize: 11
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            app.useConsole(modelData.ip)
                            consoleIpField.text = modelData.ip
                            consolePopup.close()
                        }
                    }
                }
            }

            Label {
                visible: !app.discovering && app.consoles.length === 0
                Layout.fillWidth: true
                text: qsTr("Nothing found yet. Make sure the receiver ELF is running "
                    + "on the console and both devices are on the same network.")
                color: Theme.textMuted
                font.pixelSize: 11
                wrapMode: Text.WordWrap
            }
        }
    }
}
