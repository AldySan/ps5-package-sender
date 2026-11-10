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
                         : app.receiverOnline
                           ? (app.receiverBuild.length > 0
                              ? qsTr("Receiver online (build %1)").arg(app.receiverBuild)
                              : qsTr("Receiver online"))
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

        Button {
            id: catalogButton
            visible: app.shareEnabled && app.catalogUrl.length > 0
            flat: true
            font.pixelSize: 11
            font.family: Theme.mono
            Accessible.description: qsTr("Copies the catalog address to the clipboard")
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Copy the catalog address")
            onClicked: app.copyToClipboard(app.catalogUrl)

            background: Rectangle {
                radius: Theme.controlRadius
                color: catalogButton.hovered ? Theme.accentSoft : "transparent"
                border.width: catalogButton.activeFocus ? 2 : 0
                border.color: Theme.accent
            }

            contentItem: Label {
                text: app.catalogUrl
                color: Theme.accentText
                font: catalogButton.font
                verticalAlignment: Text.AlignVCenter
            }
        }

        AppButton {
            checkable: true
            text: checked ? qsTr("Share on") : qsTr("Share to console")
            checked: app.shareEnabled
            onToggled: app.shareEnabled = checked
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Publish the library so the console browser can install on its own")
        }

        Item { Layout.fillWidth: true }

        AppComboBox {
            id: pcBox
            implicitWidth: 200
            model: app.pcAddressLabels
            visible: app.pcAddresses.length > 0
            currentIndex: app.pcIp ? app.pcAddresses.indexOf(app.pcIp) : -1
            displayText: {
                if (!app.pcIp)
                    return qsTr("PC ") + qsTr("IP")
                const idx = app.pcAddresses.indexOf(app.pcIp)
                return idx >= 0 ? qsTr("PC ") + app.pcAddressLabels[idx]
                                : qsTr("PC ") + app.pcIp
            }
            onActivated: index => app.pcIp = app.pcAddresses[index]
        }

        AppTextField {
            id: consoleIpField
            implicitWidth: 130
            placeholderText: qsTr("console IP")
            text: app.psIp
            selectByMouse: true
            onEditingFinished: app.psIp = text.trim()
            font.pixelSize: 12
        }

        AppButton {
            text: qsTr("Test")
            onClicked: app.testConnection()
        }

        AppButton {
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
        modal: true
        closePolicy: app.discovering ? Popup.NoAutoClose
                                      : (Popup.CloseOnEscape | Popup.CloseOnPressOutside)

        onOpened: app.detectConsoles()

        background: Rectangle {
            color: Theme.surface
            radius: Theme.radius
            border.width: 1
            border.color: Theme.border
        }

        Overlay.modal: Rectangle {
            color: Theme.dark ? Qt.rgba(0, 0, 0, 0.69) : Qt.rgba(0, 0, 0, 0.35)
        }

        contentItem: ColumnLayout {
            spacing: 8

            RowLayout {
                Layout.fillWidth: true

                Label {
                    text: app.discovering ? app.discoveryStatus : qsTr("Consoles")
                    color: Theme.textMuted
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                AppButton {
                    flat: true
                    implicitWidth: 30
                    font.pixelSize: 12
                    text: qsTr("✕")
                    onClicked: {
                        if (app.discovering)
                            app.cancelDiscovery()
                        consolePopup.close()
                    }
                }
            }

            BusyIndicator {
                visible: app.discovering
                running: app.discovering
                Layout.alignment: Qt.AlignHCenter
            }

            AppButton {
                visible: app.discovering
                text: qsTr("Cancel")
                Layout.alignment: Qt.AlignHCenter
                onClicked: app.cancelDiscovery()
            }

            ListView {
                id: consoleList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: app.consoles
                spacing: 6
                ScrollBar.vertical: ScrollBar {}

                delegate: ItemDelegate {
                    id: consoleEntry
                    required property var modelData
                    width: consoleList.width
                    height: 52
                    padding: 8
                    focusPolicy: Qt.StrongFocus
                    Accessible.name: modelData.ip

                    background: Rectangle {
                        radius: Theme.radius
                        color: consoleEntry.modelData.verified ? Theme.accentSoft : Theme.surfaceAlt
                        border.width: consoleEntry.modelData.ip === app.psIp ? 1
                                      : consoleEntry.activeFocus ? 2 : 0
                        border.color: Theme.accent
                    }

                    contentItem: ColumnLayout {
                        spacing: 2

                        Label {
                            text: consoleEntry.modelData.ip
                            color: Theme.text
                            font.pixelSize: 13
                            font.bold: true
                            font.family: Theme.mono
                        }

                        Label {
                            text: consoleEntry.modelData.verified
                                  ? (consoleEntry.modelData.busy ? qsTr("busy") : qsTr("ready"))
                                  : qsTr("port open, no API reply")
                            color: consoleEntry.modelData.verified ? Theme.ok : Theme.warn
                            font.pixelSize: 11
                        }
                    }

                    onClicked: {
                        app.useConsole(modelData.ip)
                        // Re-establish the text binding instead of writing
                        // into the field: an imperative assignment would
                        // detach it from app.psIp, so the field would stop
                        // following later changes and editing it could
                        // write a stale IP back into the settings.
                        consoleIpField.text = Qt.binding(() => app.psIp)
                        consolePopup.close()
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
