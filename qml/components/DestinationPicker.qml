import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import PS5PackageSender

ColumnLayout {
    id: root
    property alias label: caption.text

    spacing: 6

    Label {
        id: caption
        text: qsTr("Image copy destination")
        color: Theme.textMuted
        font.pixelSize: 11
    }

    RowLayout {
        spacing: 8

        ComboBox {
            id: pathBox
            Layout.fillWidth: true
            model: app.scanPaths
            currentIndex: {
                const idx = app.scanPaths.indexOf(app.destination)
                return idx >= 0 ? idx : -1
            }
            displayText: app.destination || qsTr("pick a scan path")
            onActivated: index => app.destination = app.scanPaths[index]

            delegate: ItemDelegate {
                required property var model
                required property int index
                width: pathBox.width
                height: 34
                highlighted: modelData === app.destination

                contentItem: Label {
                    text: modelData
                    color: modelData === app.destination ? Theme.accent : Theme.text
                    font.pixelSize: 12
                    font.family: Theme.mono
                    elide: Text.ElideMiddle
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        Button {
            text: qsTr("Add path…")
            onClicked: customDialog.open()
        }

        Button {
            visible: app.destinationIsCustom
            text: qsTr("Remove")
            onClicked: app.removeCustomScanPath(app.destination)
        }
    }

    Dialog {
        id: customDialog
        modal: true
        title: qsTr("Custom scan path")
        width: 420
        parent: Overlay.overlay
        anchors.centerIn: parent
        standardButtons: Dialog.Ok | Dialog.Cancel

        onAccepted: {
            if (!app.addCustomScanPath(customField.text.trim()))
                errorLabel.visible = true
            customField.clear()
        }
        onRejected: customField.clear()

        contentItem: ColumnLayout {
            spacing: 8

            Label {
                text: qsTr("Console-side absolute path, e.g. /mnt/usb0/homebrew. "
                    + "It must match a scanpath entry the mounter is configured "
                    + "to scan.")
                color: Theme.textMuted
                font.pixelSize: 11
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            TextField {
                id: customField
                placeholderText: "/mnt/usb0/homebrew"
                selectByMouse: true
                font.family: Theme.mono
                Layout.fillWidth: true
                onAccepted: customDialog.accept()
            }

            Label {
                id: errorLabel
                visible: false
                text: qsTr("Enter an absolute path without ..")
                color: Theme.error
                font.pixelSize: 11
            }
        }

        background: Rectangle {
            color: Theme.surface
            radius: Theme.radius
            border.width: 1
            border.color: Theme.border
        }
    }
}
