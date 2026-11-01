import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import PS5PackageSender

Rectangle {
    id: card
    required property var model
    readonly property alias selectedItem: check.checked

    color: model.selected ? Theme.accentSoft : Theme.surface
    radius: Theme.radius
    border.width: model.selected ? 1 : 0
    border.color: Theme.accent

    MouseArea {
        anchors.fill: parent
        onClicked: check.toggle()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 6

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 140
            radius: Theme.radius
            color: Theme.surfaceAlt

            Image {
                anchors.fill: parent
                anchors.margins: 6
                source: model.iconUrl ? model.iconUrl : ""
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                visible: model.iconUrl.length > 0
            }

            Label {
                anchors.centerIn: parent
                visible: model.iconUrl.length === 0
                text: model.titleId ? model.titleId.substring(0, 4) : "PKG"
                color: Theme.textMuted
                font.pixelSize: 22
                font.bold: true
            }

            CheckBox {
                id: check
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.margins: 4
                checked: model.selected
                onClicked: app.libraryModel.toggleSelected(model.id)
            }

            Rectangle {
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.margins: 4
                width: platformLabel.implicitWidth + 12
                height: platformLabel.implicitHeight + 6
                radius: 4
                color: model.platform === "PS5" ? "#2d7ff933" : "#f5b83d33"
                visible: model.platform.length > 0

                Label {
                    id: platformLabel
                    anchors.centerIn: parent
                    text: model.platform
                    color: model.platform === "PS5" ? Theme.accent : Theme.warn
                    font.pixelSize: 10
                    font.bold: true
                }
            }
        }

        Label {
            Layout.fillWidth: true
            text: model.title
            color: Theme.text
            font.pixelSize: 13
            font.bold: true
            elide: Text.ElideRight
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Label {
                text: model.titleId ? model.titleId : qsTr("unknown")
                color: Theme.textMuted
                font.pixelSize: 11
                font.family: Theme.mono
            }

            Item { Layout.fillWidth: true }

            Label {
                visible: model.version.length > 0
                text: qsTr("v") + model.version
                color: Theme.textMuted
                font.pixelSize: 11
            }

            Rectangle {
                width: kindLabel.implicitWidth + 10
                height: kindLabel.implicitHeight + 4
                radius: 4
                color: Theme.chip

                Label {
                    id: kindLabel
                    anchors.centerIn: parent
                    text: model.format.toUpperCase()
                    color: Theme.textMuted
                    font.pixelSize: 10
                    font.bold: true
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true

            Label {
                text: model.sizeText
                color: Theme.textMuted
                font.pixelSize: 11
            }

            Item { Layout.fillWidth: true }

            Button {
                visible: model.familyKey.length > 0
                flat: true
                text: "🔗"
                implicitHeight: 22
                font.pixelSize: 11
                onClicked: app.libraryModel.familyFilter = model.familyKey
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Show this title family")
            }
        }
    }
}
