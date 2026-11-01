import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import PS5PackageSender

Page {
    id: root
    background: Rectangle { color: Theme.background }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 10

        RowLayout {
            spacing: 10

            Label {
                text: app.queue ? app.queue.model.count + qsTr(" item(s)") : ""
                color: Theme.text
                font.pixelSize: 13
                font.bold: true
            }

            Label {
                visible: app.queue && app.queue.model.failedCount > 0
                text: app.queue ? app.queue.model.failedCount + qsTr(" failed") : ""
                color: Theme.error
                font.pixelSize: 12
            }

            Item { Layout.fillWidth: true }


            Button {
                text: qsTr("Retry failed")
                enabled: app.queue && app.queue.model.failedCount > 0
                onClicked: app.queue.retryFailed()
            }

            Button {
                text: qsTr("Clear finished")
                enabled: app.queue && app.queue.model.count > 0
                    && app.queue.model.activeCount < app.queue.model.count
                onClicked: app.queue.clearFinished()
            }
        }

        DestinationPicker {
            Layout.fillWidth: true
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 8
            model: app.queue ? app.queue.model : null
            ScrollBar.vertical: ScrollBar {}

            delegate: QueueRow {
                width: list.width
            }
        }

        Label {
            visible: !app.queue || app.queue.model.count === 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            text: qsTr("Nothing queued yet — select packages in the Library and press Send PKG")
            color: Theme.textMuted
            font.pixelSize: 14
            wrapMode: Text.WordWrap
        }
    }
}
